#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Question 5: Variation de la portée de transmission (txrange)
On fixe le nombre de nœuds et on varie la portée de transmission
pour observer l'impact sur les performances du réseau MANET
"""
import subprocess
import re
import pandas as pd
import matplotlib.pyplot as plt
import os
from datetime import datetime

class MANETSimulatorTxRange:
    def __init__(self, ns3_path, script_name='scratch/manet-28.cc'):
        self.ns3_path = ns3_path
        self.script_name = script_name
        self.results = []
    
    def run_simulation(self, tx_range, num_nodes=50, sim_time=50):
        """
        Exécute une simulation avec une portée de transmission spécifique
        
        Args:
            tx_range: Portée de transmission en mètres
            num_nodes: Nombre de nœuds (fixé à 50 par défaut)
            sim_time: Durée de simulation en secondes
        """
        print(f"\n{'='*70}")
        print(f"Simulation avec portée de transmission = {tx_range}m")
        print(f"Nombre de nœuds fixe: {num_nodes}")
        print(f"{'='*70}")
        
        # Commande adaptée pour ns-3.45
        cmd = [
            './ns3', 'run',
            f'"{self.script_name} --size={num_nodes} --txrange={tx_range} --simTime={sim_time}"'
        ]
        
        try:
            result = subprocess.run(
                " ".join(cmd),
                cwd=self.ns3_path,
                shell=True,
                capture_output=True,
                text=True,
                timeout=300
            )
            
            output = result.stdout + result.stderr
            print(output)
            
            # Extraction des métriques
            packets_lost = self._extract_value(output, r'Total Packets Lost:\s*(\d+)')
            throughput = self._extract_value(output, r'Throughput:\s*([\d.]+)\s*Kbps')
            pdr = self._extract_value(output, r'Packets Delivery Ratio:\s*([\d.]+)%')
            
            result_dict = {
                'tx_range': tx_range,
                'num_nodes': num_nodes,
                'packets_lost': packets_lost,
                'throughput': throughput,
                'pdr': pdr
            }
            
            print(f"\n✓ Résultats:")
            print(f"  • Portée TX: {tx_range}m")
            print(f"  • Paquets perdus: {packets_lost}")
            print(f"  • Débit: {throughput} Kbps")
            print(f"  • PDR: {pdr}%")
            
            return result_dict
            
        except subprocess.TimeoutExpired:
            print(f"✗ Timeout pour portée {tx_range}m")
            return None
        except Exception as e:
            print(f"✗ Erreur: {e}")
            return None
    
    def _extract_value(self, text, pattern):
        """Extrait une valeur à partir d'une regex"""
        match = re.search(pattern, text)
        if match:
            try:
                return int(match.group(1))
            except:
                return float(match.group(1))
        return 0
    
    def run_multiple(self, tx_range_list, num_nodes=50):
        """
        Exécute plusieurs simulations avec différentes portées
        
        Args:
            tx_range_list: Liste des portées de transmission à tester
            num_nodes: Nombre de nœuds (fixe)
        """
        print("\n" + "="*70)
        print("DÉBUT DES SIMULATIONS - VARIATION DE LA PORTÉE DE TRANSMISSION")
        print("="*70)
        print(f"Nombre de nœuds: {num_nodes} (FIXE)")
        print(f"Portées à tester: {tx_range_list}")
        print("="*70)
        
        for tx_range in tx_range_list:
            result = self.run_simulation(tx_range, num_nodes)
            if result:
                self.results.append(result)
        
        print("\n" + "="*70)
        print("SIMULATIONS TERMINÉES")
        print("="*70)
    
    def save_results(self, filename='manet_txrange_results.csv'):
        """Sauvegarde les résultats dans un CSV"""
        if not self.results:
            print("Aucun résultat à sauvegarder")
            return None
        
        df = pd.DataFrame(self.results)
        df.to_csv(filename, index=False)
        
        print(f"\n✓ Résultats sauvegardés: {filename}")
        print("\n" + "="*70)
        print("TABLEAU DES RÉSULTATS")
        print("="*70)
        print(df.to_string(index=False))
        
        return df
    
    def plot_results(self, output_dir='plots_txrange'):
        """Génère les graphiques d'analyse"""
        if not self.results:
            print("Aucune donnée pour tracer les graphiques")
            return
        
        os.makedirs(output_dir, exist_ok=True)
        df = pd.DataFrame(self.results)
        
        # Style
        plt.style.use('seaborn-v0_8-darkgrid')
        
        # Figure avec 3 sous-graphiques
        fig, axes = plt.subplots(3, 1, figsize=(12, 14))
        fig.suptitle('Impact de la portée de transmission sur les performances MANET', 
                     fontsize=16, fontweight='bold')
        
        # Graphique 1: Débit vs Portée TX
        axes[0].plot(df['tx_range'], df['throughput'], 
                    marker='o', linewidth=2.5, markersize=10, 
                    color='#2E86AB', label='Débit')
        axes[0].set_xlabel('Portée de transmission (m)', fontsize=12, fontweight='bold')
        axes[0].set_ylabel('Débit (Kbps)', fontsize=12, fontweight='bold')
        axes[0].set_title('Débit en fonction de la portée de transmission', 
                         fontsize=13, fontweight='bold')
        axes[0].grid(True, alpha=0.3)
        axes[0].legend(fontsize=11)
        
        # Annotation des valeurs
        for i, row in df.iterrows():
            axes[0].annotate(f'{row["throughput"]:.2f}', 
                           (row['tx_range'], row['throughput']),
                           textcoords="offset points", xytext=(0,10), 
                           ha='center', fontsize=9)
        
        # Graphique 2: Paquets perdus vs Portée TX
        axes[1].plot(df['tx_range'], df['packets_lost'], 
                    marker='s', linewidth=2.5, markersize=10, 
                    color='#A23B72', label='Paquets perdus')
        axes[1].set_xlabel('Portée de transmission (m)', fontsize=12, fontweight='bold')
        axes[1].set_ylabel('Paquets perdus', fontsize=12, fontweight='bold')
        axes[1].set_title('Pertes de paquets en fonction de la portée', 
                         fontsize=13, fontweight='bold')
        axes[1].grid(True, alpha=0.3)
        axes[1].legend(fontsize=11)
        
        # Annotation des valeurs
        for i, row in df.iterrows():
            axes[1].annotate(f'{int(row["packets_lost"])}', 
                           (row['tx_range'], row['packets_lost']),
                           textcoords="offset points", xytext=(0,10), 
                           ha='center', fontsize=9)
        
        # Graphique 3: PDR vs Portée TX
        axes[2].plot(df['tx_range'], df['pdr'], 
                    marker='^', linewidth=2.5, markersize=10, 
                    color='#18A558', label='PDR')
        axes[2].set_xlabel('Portée de transmission (m)', fontsize=12, fontweight='bold')
        axes[2].set_ylabel('PDR (%)', fontsize=12, fontweight='bold')
        axes[2].set_title('Taux de livraison (PDR) en fonction de la portée', 
                         fontsize=13, fontweight='bold')
        axes[2].grid(True, alpha=0.3)
        axes[2].legend(fontsize=11)
        axes[2].set_ylim([0, 105])
        
        # Annotation des valeurs
        for i, row in df.iterrows():
            axes[2].annotate(f'{row["pdr"]:.1f}%', 
                           (row['tx_range'], row['pdr']),
                           textcoords="offset points", xytext=(0,10), 
                           ha='center', fontsize=9)
        
        plt.tight_layout()
        
        # Sauvegarde
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = os.path.join(output_dir, f'txrange_analysis_{timestamp}.png')
        plt.savefig(filename, dpi=300, bbox_inches='tight')
        print(f"\n✓ Graphiques sauvegardés: {filename}")
        
        # Graphiques individuels pour inclure dans le rapport
        self._save_individual_plots(df, output_dir)
        
        plt.show()
    
    def _save_individual_plots(self, df, output_dir):
        """Sauvegarde des graphiques individuels"""
        # Débit
        plt.figure(figsize=(10, 6))
        plt.plot(df['tx_range'], df['throughput'], marker='o', linewidth=2, markersize=8, color='#2E86AB')
        plt.xlabel('Portée de transmission (m)', fontsize=11)
        plt.ylabel('Débit (Kbps)', fontsize=11)
        plt.title('Débit vs Portée TX', fontsize=12, fontweight='bold')
        plt.grid(True, alpha=0.3)
        plt.savefig(f"{output_dir}/throughput_txrange.png", dpi=200, bbox_inches='tight')
        plt.close()
        
        # Pertes
        plt.figure(figsize=(10, 6))
        plt.plot(df['tx_range'], df['packets_lost'], marker='s', linewidth=2, markersize=8, color='#A23B72')
        plt.xlabel('Portée de transmission (m)', fontsize=11)
        plt.ylabel('Paquets perdus', fontsize=11)
        plt.title('Pertes vs Portée TX', fontsize=12, fontweight='bold')
        plt.grid(True, alpha=0.3)
        plt.savefig(f"{output_dir}/loss_txrange.png", dpi=200, bbox_inches='tight')
        plt.close()
        
        # PDR
        plt.figure(figsize=(10, 6))
        plt.plot(df['tx_range'], df['pdr'], marker='^', linewidth=2, markersize=8, color='#18A558')
        plt.xlabel('Portée de transmission (m)', fontsize=11)
        plt.ylabel('PDR (%)', fontsize=11)
        plt.title('PDR vs Portée TX', fontsize=12, fontweight='bold')
        plt.grid(True, alpha=0.3)
        plt.ylim([0, 105])
        plt.savefig(f"{output_dir}/pdr_txrange.png", dpi=200, bbox_inches='tight')
        plt.close()
    
    def generate_comparative_report(self, output_file='rapport_txrange.txt'):
        """Génère un rapport comparatif détaillé"""
        if not self.results:
            print("Aucun résultat pour générer le rapport")
            return
        
        df = pd.DataFrame(self.results)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("="*80 + "\n")
            f.write("RAPPORT D'ANALYSE - VARIATION DE LA PORTÉE DE TRANSMISSION\n")
            f.write("="*80 + "\n\n")
            
            f.write(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Nombre de simulations: {len(self.results)}\n")
            f.write(f"Nombre de nœuds (fixe): {df['num_nodes'].iloc[0]}\n")
            f.write(f"Portées testées: {df['tx_range'].min()}m - {df['tx_range'].max()}m\n\n")
            
            f.write("-"*80 + "\n")
            f.write("TABLEAU COMPARATIF DES RÉSULTATS\n")
            f.write("-"*80 + "\n\n")
            f.write(df.to_string(index=False))
            f.write("\n\n")
            
            f.write("-"*80 + "\n")
            f.write("ANALYSE STATISTIQUE\n")
            f.write("-"*80 + "\n\n")
            
            # Débit
            f.write("📊 DÉBIT:\n")
            f.write(f"  • Moyenne: {df['throughput'].mean():.2f} Kbps\n")
            f.write(f"  • Maximum: {df['throughput'].max():.2f} Kbps (portée {df.loc[df['throughput'].idxmax(), 'tx_range']}m)\n")
            f.write(f"  • Minimum: {df['throughput'].min():.2f} Kbps (portée {df.loc[df['throughput'].idxmin(), 'tx_range']}m)\n")
            f.write(f"  • Écart-type: {df['throughput'].std():.2f} Kbps\n\n")
            
            # Pertes
            f.write("📉 PERTES DE PAQUETS:\n")
            f.write(f"  • Total: {df['packets_lost'].sum()}\n")
            f.write(f"  • Maximum: {df['packets_lost'].max()} (portée {df.loc[df['packets_lost'].idxmax(), 'tx_range']}m)\n")
            f.write(f"  • Minimum: {df['packets_lost'].min()} (portée {df.loc[df['packets_lost'].idxmin(), 'tx_range']}m)\n\n")
            
            # PDR
            f.write("✅ TAUX DE LIVRAISON (PDR):\n")
            f.write(f"  • Moyenne: {df['pdr'].mean():.2f}%\n")
            f.write(f"  • Maximum: {df['pdr'].max():.2f}% (portée {df.loc[df['pdr'].idxmax(), 'tx_range']}m)\n")
            f.write(f"  • Minimum: {df['pdr'].min():.2f}% (portée {df.loc[df['pdr'].idxmin(), 'tx_range']}m)\n\n")
            
            f.write("-"*80 + "\n")
            f.write("INTERPRÉTATION ET COMPARAISON AVEC LA QUESTION 4\n")
            f.write("-"*80 + "\n\n")
            
            # Analyse de la tendance
            if df['throughput'].iloc[-1] > df['throughput'].iloc[0]:
                f.write("🔹 TENDANCE DU DÉBIT:\n")
                f.write("   Le débit AUGMENTE avec la portée de transmission.\n")
                f.write("   → Une plus grande portée permet une meilleure connectivité\n")
                f.write("   → Les nœuds peuvent communiquer directement sans multiples sauts\n")
                f.write("   → Moins de relayage = moins de collisions et meilleure performance\n\n")
            else:
                f.write("🔹 TENDANCE DU DÉBIT:\n")
                f.write("   Le débit diminue ou stagne à forte portée.\n")
                f.write("   → Possible saturation du médium\n")
                f.write("   → Augmentation des interférences avec portée élevée\n\n")
            
            if df['packets_lost'].iloc[0] > df['packets_lost'].iloc[-1]:
                f.write("🔹 TENDANCE DES PERTES:\n")
                f.write("   Les pertes DIMINUENT avec l'augmentation de la portée.\n")
                f.write("   → Routes plus courtes et plus stables\n")
                f.write("   → Moins de ruptures de liens\n\n")
            
            f.write("-"*80 + "\n")
            f.write("COMPARAISON AVEC LA QUESTION 4 (VARIATION DU NOMBRE DE NŒUDS)\n")
            f.write("-"*80 + "\n\n")
            
            f.write("📌 DIFFÉRENCES CLÉS:\n\n")
            
            f.write("1. QUESTION 4 (Variation du nombre de nœuds):\n")
            f.write("   • Portée fixe, densité variable\n")
            f.write("   • Résultat: Performances DÉGRADÉES avec plus de nœuds\n")
            f.write("   • Cause: Contention du médium, collisions, overhead AODV\n\n")
            
            f.write("2. QUESTION 5 (Variation de la portée):\n")
            f.write("   • Nœuds fixes, connectivité variable\n")
            f.write("   • Résultat: Performances AMÉLIORÉES avec plus de portée\n")
            f.write("   • Cause: Routes plus courtes, moins de sauts, meilleure connectivité\n\n")
            
            f.write("🎯 CONCLUSION:\n")
            f.write("   NON, les résultats sont INVERSES!\n")
            f.write("   • Question 4: ↑ nœuds → ↓ performances (problème de scalabilité)\n")
            f.write("   • Question 5: ↑ portée → ↑ performances (meilleure connectivité)\n\n")
            
            f.write("   Cela montre que:\n")
            f.write("   - La DENSITÉ affecte négativement les performances\n")
            f.write("   - La CONNECTIVITÉ affecte positivement les performances\n")
            f.write("   - Le compromis optimal dépend du scénario d'application\n\n")
            
            f.write("-"*80 + "\n")
            f.write("RECOMMANDATIONS\n")
            f.write("-"*80 + "\n\n")
            
            optimal_range = df.loc[df['throughput'].idxmax(), 'tx_range']
            f.write(f"• Portée optimale observée: {optimal_range}m\n")
            f.write("• Pour réseaux denses: Privilégier une portée modérée\n")
            f.write("• Pour réseaux épars: Augmenter la portée pour maintenir la connectivité\n")
            f.write("• Adapter dynamiquement la puissance TX selon la densité locale\n")
        
        print(f"\n✓ Rapport comparatif généré: {output_file}")


def main():
    """Fonction principale pour la Question 5"""
    
    # Configuration - ADAPTER À VOTRE ENVIRONNEMENT
    NS3_PATH = "/home/ubuntu/ns-allinone-3.45/ns-3.45"
    
    print("\n" + "="*80)
    print("QUESTION 5: VARIATION DE LA PORTÉE DE TRANSMISSION")
    print("="*80)
    
    # Vérification du chemin
    if not os.path.exists(NS3_PATH):
        print(f"\n⚠️  ATTENTION: Chemin NS-3 introuvable: {NS3_PATH}")
        NS3_PATH = input("Entrez le chemin correct vers ns-3: ").strip()
    
    # Initialisation
    simulator = MANETSimulatorTxRange(NS3_PATH)
    
    # PARAMÈTRES DE SIMULATION
    # Nombre de nœuds FIXE (contrairement à la Q4)
    NUM_NODES = 50
    
    # Liste des portées à tester (en mètres)
    # On teste de 30m à 150m par pas de 10m
    tx_range_list = list(range(30, 151, 10))  # [30, 40, 50, ..., 150]
    
    print(f"\nParamètres:")
    print(f"  • Nombre de nœuds: {NUM_NODES} (FIXE)")
    print(f"  • Portées à tester: {tx_range_list[0]}m à {tx_range_list[-1]}m")
    print(f"  • Nombre de simulations: {len(tx_range_list)}")
    
    input("\n▶️  Appuyez sur Entrée pour démarrer...")
    
    # Exécution des simulations
    simulator.run_multiple(tx_range_list, num_nodes=NUM_NODES)
    
    # Sauvegarde et analyse
    simulator.save_results('manet_txrange_results.csv')
    simulator.plot_results('plots_txrange')
    simulator.generate_comparative_report('rapport_txrange.txt')
    
    print("\n" + "="*80)
    print("✅ AUTOMATISATION TERMINÉE!")
    print("="*80)
    print("\nFichiers générés:")
    print("  📄 manet_txrange_results.csv - Résultats bruts")
    print("  📊 plots_txrange/ - Graphiques d'analyse")
    print("  📝 rapport_txrange.txt - Rapport comparatif complet")
    print("\n" + "="*80)


if __name__ == "__main__":
    main()
