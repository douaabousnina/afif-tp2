#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess
import re
import pandas as pd
import matplotlib.pyplot as plt
import os
from datetime import datetime

class MANETSimulator:
    def __init__(self, ns3_path, script_name='scratch/manet-28.cc'):
        self.ns3_path = ns3_path
        self.script_name = script_name
        self.results = []

    def run_simulation(self, num_nodes, tx_range=50, sim_time=50):
        print(f"\n=== Simulation avec {num_nodes} nœuds ===")

        # Commande adaptée à TON environnement
        cmd = [
            './ns3', 'run',
            f'"{self.script_name} --size={num_nodes} --txrange={tx_range} --simTime={sim_time}"'
        ]

        try:
            # IMPORTANT : shell=True obligatoire pour ns3
            result = subprocess.run(
                " ".join(cmd),
                cwd=self.ns3_path,
                shell=True,
                capture_output=True,
                text=True
            )

            output = result.stdout + result.stderr
            print(output)

            packets_lost = self._extract_value(output, r'Total Packets Lost:\s*(\d+)')
            throughput = self._extract_value(output, r'Throughput:\s*([\d.]+)\s*Kbps')
            pdr = self._extract_value(output, r'Packets Delivery Ratio:\s*([\d.]+)%')

            result_dict = {
                'num_nodes': num_nodes,
                'packets_lost': packets_lost,
                'throughput': throughput,
                'pdr': pdr
            }

            return result_dict

        except Exception as e:
            print(f"ERREUR : {e}")
            return None

    def _extract_value(self, text, pattern):
        match = re.search(pattern, text)
        if match:
            try:
                return int(match.group(1))
            except:
                return float(match.group(1))
        return 0

    def run_multiple(self, node_list):
        for n in node_list:
            result = self.run_simulation(n)
            if result:
                self.results.append(result)

    def save_results(self):
        df = pd.DataFrame(self.results)
        df.to_csv("manet_results.csv", index=False)
        print("\nRésultats enregistrés dans manet_results.csv")
        print(df)
        return df

    def plot_results(self):
        df = pd.DataFrame(self.results)

        if df.empty:
            print("Aucune donnée pour tracer les graphiques.")
            return

        plt.figure(figsize=(10, 6))
        plt.plot(df['num_nodes'], df['throughput'], marker='o')
        plt.xlabel('Nombre de nœuds')
        plt.ylabel('Débit (Kbps)')
        plt.title('Débit en fonction du nombre de nœuds')
        plt.grid()
        plt.savefig("throughput_plot.png")
        plt.close()

        plt.figure(figsize=(10, 6))
        plt.plot(df['num_nodes'], df['pdr'], marker='s')
        plt.xlabel('Nombre de nœuds')
        plt.ylabel('PDR (%)')
        plt.title('Taux de livraison des paquets')
        plt.grid()
        plt.savefig("pdr_plot.png")
        plt.close()

        plt.figure(figsize=(10, 6))
        plt.plot(df['num_nodes'], df['packets_lost'], marker='^')
        plt.xlabel('Nombre de nœuds')
        plt.ylabel('Paquets perdus')
        plt.title('Paquets perdus vs nombre de nœuds')
        plt.grid()
        plt.savefig("loss_plot.png")
        plt.close()

        print("\nGraphiques générés : throughput_plot.png, pdr_plot.png, loss_plot.png")

def main():
    #  Chemin EXACT vers TON NS-3
    NS3_PATH = "/home/ubuntu/ns-allinone-3.45/ns-3.45"

    sim = MANETSimulator(NS3_PATH)

    # Liste des tailles à simuler
    node_list = list(range(10, 101, 10))  # 10 → 100

    sim.run_multiple(node_list)
    sim.save_results()
    sim.plot_results()

if __name__ == "__main__":
    main()
