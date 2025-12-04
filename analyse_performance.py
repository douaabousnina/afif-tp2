#!/usr/bin/env python3
"""
Script pour varier la topologie et analyser les performances
Pour le TP2 - Question 2.d
Version FINALE - Analyse uniquement le trafic UDP applicatif
"""

import subprocess
import os
import matplotlib.pyplot as plt
import numpy as np
import time
import re

class TopologyAnalyzer:
    def __init__(self, ns3_path="~/ns-allinone-3.45/ns-3.45"):
        self.ns3_path = os.path.expanduser(ns3_path)
        self.results = {
            'wifi': {'nodes': [], 'throughput': [], 'delay': [], 'pdr': [], 'lost': []},
            'csma': {'nodes': [], 'throughput': [], 'delay': [], 'pdr': [], 'lost': []}
        }
    
    def run_simulation(self, nWifi, nCsma):
        """Exécute une simulation NS-3"""
        
        cmd = f"./ns3 run 'scratch/third2 --nWifi={nWifi} --nCsma={nCsma} --tracing=true --verbose=false'"
        
        print(f"Running: nWifi={nWifi}, nCsma={nCsma}...", end=" ")
        
        try:
            result = subprocess.run(
                cmd,
                shell=True,
                capture_output=True,
                text=True,
                cwd=self.ns3_path,
                timeout=30
            )
            
            # Extraire les stats FlowMonitor de la sortie
            output = result.stdout
            flowmon_stats = self.parse_flowmonitor_output(output)
            
            print("✓")
            return True, flowmon_stats
        except subprocess.TimeoutExpired:
            print("✗ (timeout)")
            return False, None
        except Exception as e:
            print(f"✗ ({e})")
            return False, None
    
    def parse_flowmonitor_output(self, output):
        """Parse la sortie FlowMonitor de NS-3"""
        
        stats = {
            'tx_packets': 0,
            'rx_packets': 0,
            'lost_packets': 0,
            'throughput': 0.0,
            'delay': 0.0
        }
        
        try:
            # Rechercher les statistiques dans la sortie
            tx_match = re.search(r'Tx Packets:\s+(\d+)', output)
            rx_match = re.search(r'Rx Packets:\s+(\d+)', output)
            lost_match = re.search(r'Lost Packets:\s+(\d+)', output)
            throughput_match = re.search(r'Throughput:\s+([\d.]+)\s+Kbps', output)
            delay_match = re.search(r'Mean Delay:\s+([\d.]+)\s+ms', output)
            
            if tx_match:
                stats['tx_packets'] = int(tx_match.group(1))
            if rx_match:
                stats['rx_packets'] = int(rx_match.group(1))
            if lost_match:
                stats['lost_packets'] = int(lost_match.group(1))
            if throughput_match:
                stats['throughput'] = float(throughput_match.group(1))
            if delay_match:
                stats['delay'] = float(delay_match.group(1))
                
        except Exception as e:
            print(f"    [Warning] Failed to parse FlowMonitor: {e}")
        
        return stats
    
    def analyze_trace_file_udp_only(self, filename="tracemetrics.tr"):
        """Analyse UNIQUEMENT les paquets UDP (port 9 - Echo)"""
        
        filepath = os.path.join(self.ns3_path, filename)
        
        if not os.path.exists(filepath):
            print(f"    [Warning] {filename} not found")
            return None
        
        # Filtrer UNIQUEMENT les paquets UDP Echo (port 9)
        udp_packets_tx = {}  # uid -> (time, size)
        udp_packets_rx = {}  # uid -> (time, size)
        
        tx_count = 0
        rx_count = 0
        total_bytes_tx = 0
        total_bytes_rx = 0
        delays = []
        
        start_time = float('inf')
        end_time = 0
        
        try:
            with open(filepath, 'r') as f:
                for line in f:
                    line_lower = line.lower()
                    
                    # Filtrer UNIQUEMENT les lignes contenant du trafic UDP sur port 9
                    # Ignorer les trames WiFi management, ARP, etc.
                    if 'udp' not in line_lower:
                        continue
                    
                    # Vérifier que c'est bien le port 9 (Echo)
                    if ':9 >' not in line and ':9>' not in line and 'dst=9' not in line_lower:
                        continue
                    
                    parts = line.split()
                    if len(parts) < 2:
                        continue
                    
                    event = parts[0]
                    
                    try:
                        time_val = float(parts[1])
                    except ValueError:
                        continue
                    
                    start_time = min(start_time, time_val)
                    end_time = max(end_time, time_val)
                    
                    # Extraire taille et UID
                    size_match = re.search(r'length[:\s]+(\d+)', line)
                    packet_size = int(size_match.group(1)) if size_match else 1024
                    
                    uid_match = re.search(r'ns3::Packet[:\s]+(\d+)', line)
                    packet_uid = uid_match.group(1) if uid_match else None
                    
                    # Compter uniquement + (enqueue) et r (receive)
                    if event == '+':  # Transmission
                        tx_count += 1
                        total_bytes_tx += packet_size
                        if packet_uid:
                            udp_packets_tx[packet_uid] = (time_val, packet_size)
                    
                    elif event == 'r':  # Réception
                        rx_count += 1
                        total_bytes_rx += packet_size
                        if packet_uid:
                            udp_packets_rx[packet_uid] = (time_val, packet_size)
                            
                            if packet_uid in udp_packets_tx:
                                tx_time, _ = udp_packets_tx[packet_uid]
                                delay = (time_val - tx_time) * 1000
                                if 0 < delay < 1000:  # Filtrer aberrations
                                    delays.append(delay)
            
            # Calculer métriques
            lost = tx_count - rx_count
            pdr = (rx_count / tx_count * 100) if tx_count > 0 else 0
            avg_delay = np.mean(delays) if delays else 0
            
            duration = end_time - start_time if end_time > start_time else 1.0
            throughput = (total_bytes_rx * 8) / (duration * 1000) if duration > 0 else 0
            
            return {
                'throughput': throughput,
                'delay': avg_delay,
                'pdr': pdr,
                'lost': max(0, lost),  # Pas de pertes négatives
                'tx_packets': tx_count,
                'rx_packets': rx_count,
                'duration': duration
            }
            
        except Exception as e:
            print(f"    [Error] Trace analysis failed: {e}")
            return None
    
    def vary_wifi_nodes(self, wifi_range=range(1, 10), nCsma_fixed=3):
        """Varie le nombre de nœuds WiFi"""
        
        print("\n" + "="*60)
        print("VARIATION DU NOMBRE DE NŒUDS WiFi")
        print("="*60)
        
        for nWifi in wifi_range:
            success, flowmon_stats = self.run_simulation(nWifi, nCsma_fixed)
            
            if success:
                # Priorité 1: FlowMonitor (plus précis)
                if flowmon_stats and flowmon_stats['throughput'] > 0:
                    metrics = flowmon_stats
                    tx = metrics['tx_packets']
                    rx = metrics['rx_packets']
                    lost = metrics['lost_packets']
                    pdr = (rx / tx * 100) if tx > 0 else 0
                    
                    metrics['pdr'] = pdr
                    metrics['lost'] = lost
                    
                    print(f"    [FlowMonitor] TX:{tx}, RX:{rx}, Lost:{lost}, PDR:{pdr:.1f}%")
                
                # Priorité 2: Trace file (fallback)
                else:
                    metrics = self.analyze_trace_file_udp_only()
                    
                    if metrics:
                        print(f"    [Trace] TX:{metrics['tx_packets']}, RX:{metrics['rx_packets']}, "
                              f"Lost:{metrics['lost']}, PDR:{metrics['pdr']:.1f}%")
                
                # Enregistrer résultats
                if metrics and metrics['throughput'] >= 0:
                    self.results['wifi']['nodes'].append(nWifi)
                    self.results['wifi']['throughput'].append(metrics['throughput'])
                    self.results['wifi']['delay'].append(metrics['delay'])
                    self.results['wifi']['pdr'].append(metrics['pdr'])
                    self.results['wifi']['lost'].append(metrics['lost'])
                    
                    print(f"  → Throughput: {metrics['throughput']:.2f} Kbps, "
                          f"Delay: {metrics['delay']:.2f} ms, PDR: {metrics['pdr']:.2f}%\n")
            
            time.sleep(0.5)
        
        if len(self.results['wifi']['nodes']) > 0:
            self.plot_wifi_results()
    
    def vary_csma_nodes(self, csma_range=range(1, 10), nWifi_fixed=3):
        """Varie le nombre de nœuds CSMA"""
        
        print("\n" + "="*60)
        print("VARIATION DU NOMBRE DE NŒUDS CSMA")
        print("="*60)
        
        for nCsma in csma_range:
            success, flowmon_stats = self.run_simulation(nWifi_fixed, nCsma)
            
            if success:
                if flowmon_stats and flowmon_stats['throughput'] > 0:
                    metrics = flowmon_stats
                    tx = metrics['tx_packets']
                    rx = metrics['rx_packets']
                    lost = metrics['lost_packets']
                    pdr = (rx / tx * 100) if tx > 0 else 0
                    
                    metrics['pdr'] = pdr
                    metrics['lost'] = lost
                    
                    print(f"    [FlowMonitor] TX:{tx}, RX:{rx}, Lost:{lost}, PDR:{pdr:.1f}%")
                else:
                    metrics = self.analyze_trace_file_udp_only()
                    
                    if metrics:
                        print(f"    [Trace] TX:{metrics['tx_packets']}, RX:{metrics['rx_packets']}, "
                              f"Lost:{metrics['lost']}, PDR:{metrics['pdr']:.1f}%")
                
                if metrics and metrics['throughput'] >= 0:
                    self.results['csma']['nodes'].append(nCsma)
                    self.results['csma']['throughput'].append(metrics['throughput'])
                    self.results['csma']['delay'].append(metrics['delay'])
                    self.results['csma']['pdr'].append(metrics['pdr'])
                    self.results['csma']['lost'].append(metrics['lost'])
                    
                    print(f"  → Throughput: {metrics['throughput']:.2f} Kbps, "
                          f"Delay: {metrics['delay']:.2f} ms, PDR: {metrics['pdr']:.2f}%\n")
            
            time.sleep(0.5)
        
        if len(self.results['csma']['nodes']) > 0:
            self.plot_csma_results()
    
    def plot_wifi_results(self):
        """Trace les courbes pour variation WiFi"""
        
        data = self.results['wifi']
        
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Impact de la variation du nombre de nœuds WiFi', 
                     fontsize=16, fontweight='bold')
        
        # Débit
        axes[0, 0].plot(data['nodes'], data['throughput'], 'b-o', linewidth=2, markersize=8)
        axes[0, 0].set_xlabel('Nombre de nœuds WiFi', fontsize=12)
        axes[0, 0].set_ylabel('Débit (Kbps)', fontsize=12)
        axes[0, 0].set_title('Débit vs Nombre de nœuds WiFi')
        axes[0, 0].grid(True, alpha=0.3)
        
        # Délai
        axes[0, 1].plot(data['nodes'], data['delay'], 'r-s', linewidth=2, markersize=8)
        axes[0, 1].set_xlabel('Nombre de nœuds WiFi', fontsize=12)
        axes[0, 1].set_ylabel('Délai moyen (ms)', fontsize=12)
        axes[0, 1].set_title('Délai vs Nombre de nœuds WiFi')
        axes[0, 1].grid(True, alpha=0.3)
        
        # PDR
        axes[1, 0].plot(data['nodes'], data['pdr'], 'g-^', linewidth=2, markersize=8)
        axes[1, 0].set_xlabel('Nombre de nœuds WiFi', fontsize=12)
        axes[1, 0].set_ylabel('Taux de livraison (%)', fontsize=12)
        axes[1, 0].set_title('PDR vs Nombre de nœuds WiFi')
        axes[1, 0].set_ylim([0, 105])
        axes[1, 0].grid(True, alpha=0.3)
        
        # Pertes
        axes[1, 1].plot(data['nodes'], data['lost'], 'm-d', linewidth=2, markersize=8)
        axes[1, 1].set_xlabel('Nombre de nœuds WiFi', fontsize=12)
        axes[1, 1].set_ylabel('Paquets perdus', fontsize=12)
        axes[1, 1].set_title('Pertes vs Nombre de nœuds WiFi')
        axes[1, 1].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig('wifi_topology_variation.png', dpi=300, bbox_inches='tight')
        print("\n✓ Graphique sauvegardé: wifi_topology_variation.png")
        plt.show()
    
    def plot_csma_results(self):
        """Trace les courbes pour variation CSMA"""
        
        data = self.results['csma']
        
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Impact de la variation du nombre de nœuds CSMA', 
                     fontsize=16, fontweight='bold')
        
        # Débit
        axes[0, 0].plot(data['nodes'], data['throughput'], 'b-o', linewidth=2, markersize=8)
        axes[0, 0].set_xlabel('Nombre de nœuds CSMA', fontsize=12)
        axes[0, 0].set_ylabel('Débit (Kbps)', fontsize=12)
        axes[0, 0].set_title('Débit vs Nombre de nœuds CSMA')
        axes[0, 0].grid(True, alpha=0.3)
        
        # Délai
        axes[0, 1].plot(data['nodes'], data['delay'], 'r-s', linewidth=2, markersize=8)
        axes[0, 1].set_xlabel('Nombre de nœuds CSMA', fontsize=12)
        axes[0, 1].set_ylabel('Délai moyen (ms)', fontsize=12)
        axes[0, 1].set_title('Délai vs Nombre de nœuds CSMA')
        axes[0, 1].grid(True, alpha=0.3)
        
        # PDR
        axes[1, 0].plot(data['nodes'], data['pdr'], 'g-^', linewidth=2, markersize=8)
        axes[1, 0].set_xlabel('Nombre de nœuds CSMA', fontsize=12)
        axes[1, 0].set_ylabel('Taux de livraison (%)', fontsize=12)
        axes[1, 0].set_title('PDR vs Nombre de nœuds CSMA')
        axes[1, 0].set_ylim([0, 105])
        axes[1, 0].grid(True, alpha=0.3)
        
        # Pertes
        axes[1, 1].plot(data['nodes'], data['lost'], 'm-d', linewidth=2, markersize=8)
        axes[1, 1].set_xlabel('Nombre de nœuds CSMA', fontsize=12)
        axes[1, 1].set_ylabel('Paquets perdus', fontsize=12)
        axes[1, 1].set_title('Pertes vs Nombre de nœuds CSMA')
        axes[1, 1].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig('csma_topology_variation.png', dpi=300, bbox_inches='tight')
        print("\n✓ Graphique sauvegardé: csma_topology_variation.png")
        plt.show()
    
    def generate_report(self):
        """Génère un rapport complet"""
        
        report = """
╔════════════════════════════════════════════════════════════════╗
║           RAPPORT D'ANALYSE - VARIATION DE TOPOLOGIE           ║
║              TP2 - Protocoles des Réseaux Mobiles             ║
╚════════════════════════════════════════════════════════════════╝

"""
        
        if len(self.results['wifi']['nodes']) > 0:
            report += "VARIATION WIFI:\n"
            report += "-" * 60 + "\n"
            report += f"Configurations testées: {len(self.results['wifi']['nodes'])}\n"
            report += f"Plage: {min(self.results['wifi']['nodes'])} à {max(self.results['wifi']['nodes'])} nœuds\n\n"
            report += f"Débit moyen: {np.mean(self.results['wifi']['throughput']):.2f} Kbps\n"
            report += f"Débit min: {min(self.results['wifi']['throughput']):.2f} Kbps\n"
            report += f"Débit max: {max(self.results['wifi']['throughput']):.2f} Kbps\n"
            report += f"Délai moyen: {np.mean(self.results['wifi']['delay']):.2f} ms\n"
            report += f"PDR moyen: {np.mean(self.results['wifi']['pdr']):.2f}%\n\n"
        
        if len(self.results['csma']['nodes']) > 0:
            report += "VARIATION CSMA:\n"
            report += "-" * 60 + "\n"
            report += f"Configurations testées: {len(self.results['csma']['nodes'])}\n"
            report += f"Plage: {min(self.results['csma']['nodes'])} à {max(self.results['csma']['nodes'])} nœuds\n\n"
            report += f"Débit moyen: {np.mean(self.results['csma']['throughput']):.2f} Kbps\n"
            report += f"Débit min: {min(self.results['csma']['throughput']):.2f} Kbps\n"
            report += f"Débit max: {max(self.results['csma']['throughput']):.2f} Kbps\n"
            report += f"Délai moyen: {np.mean(self.results['csma']['delay']):.2f} ms\n"
            report += f"PDR moyen: {np.mean(self.results['csma']['pdr']):.2f}%\n\n"
        
        report += """
INTERPRÉTATIONS:
----------------
1. WiFi - Impact de la densité des nœuds:
   • Plus de nœuds → Plus de contention sur le canal
   • Augmentation des collisions et retransmissions
   • Mobilité RandomWalk2D → Variations de signal
   • Débit peut diminuer avec la congestion

2. CSMA - Performance sur réseau filaire:
   • Domaine de collision contrôlé
   • Performance plus stable que WiFi
   • Impact modéré de l'ajout de nœuds
   • Délai relativement constant

"""        
        with open('topology_analysis_report.txt', 'w') as f:
            f.write(report)
        
        print(report)
        print("\n✓ Rapport sauvegardé: topology_analysis_report.txt")


def main():
    print("""
╔═══════════════════════════════════════════════════════════════╗
║    Analyse de variation de topologie - TP2 RMOB             ║
║           Université de Carthage - INSAT                     ║
╚═══════════════════════════════════════════════════════════════╝
    """)
    
    # Chemin NS-3
    ns3_path = input(f"Chemin NS-3 [~/ns-allinone-3.45/ns-3.45]: ").strip()
    if not ns3_path:
        ns3_path = "~/ns-allinone-3.45/ns-3.45"
    
    analyzer = TopologyAnalyzer(ns3_path)
    
    print("\nOptions:")
    print("1. Varier WiFi (1-9 nœuds)")
    print("2. Varier CSMA (1-9 nœuds)")
    print("3. Les deux")
    
    choice = input("\nVotre choix (1-3): ").strip()
    
    if choice == '1':
        analyzer.vary_wifi_nodes(range(1, 10), 3)
        analyzer.generate_report()
    
    elif choice == '2':
        analyzer.vary_csma_nodes(range(1, 10), 3)
        analyzer.generate_report()
    
    elif choice == '3':
        analyzer.vary_wifi_nodes(range(1, 10), 3)
        analyzer.vary_csma_nodes(range(1, 10), 3)
        analyzer.generate_report()
    
    else:
        print("Choix invalide!")
        return
    
    print("\n✓ Analyse terminée!")


if __name__ == "__main__":
    main()
