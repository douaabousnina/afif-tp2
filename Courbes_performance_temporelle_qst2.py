import matplotlib.pyplot as plt
import subprocess
import os
import re
import numpy as np

# Demander le mode à l'utilisateur
print("=== Sélection du mode ===")
print("1. low")
print("2. medium")
print("3. high")
print("=========================")

while True:
    choix = input("Entrez le numéro du mode (1-3) ou le nom du mode : ").strip().lower()
    
    if choix == "1" or choix == "low":
        mode = "low"
        break
    elif choix == "2" or choix == "medium":
        mode = "medium"
        break
    elif choix == "3" or choix == "high":
        mode = "high"
        break
    else:
        print(" Choix invalide ! Veuillez entrer 1, 2, 3, 4 ou le nom du mode.")

print(f"\nLancement avec mode = {mode} ...\n")
cmd = f"./ns3 run \"scratch/third2 --mode={mode}\" 2>&1 | tee simulation_output.log"
subprocess.run(cmd, shell=True)

packets = []        # (tx_time, rx_time, size_bytes)
tx_times = []
rx_times = []
sizes = []

with open("simulation_output.log", "r") as f:
    lines = f.readlines()

for line in lines:
    # 1. Cas UdpEcho (low / medium / high)
    m_tx = re.search(r'At time \+([\d\.]+)s client sent (\d+) bytes', line)
    m_rx = re.search(r'At time \+([\d\.]+)s client received (\d+) bytes', line)
    if m_tx:
        tx_times.append(float(m_tx.group(1)))
        sizes.append(int(m_tx.group(2)))
    if m_rx:
        rx_times.append(float(m_rx.group(1)))

rx_times = sorted(rx_times)
tx_times = sorted(tx_times)

paired = []
i = j = 0
while i < len(tx_times) and j < len(rx_times):
    if tx_times[i] < rx_times[j]:
        paired.append((tx_times[i], rx_times[j], sizes[i] if i < len(sizes) else 1024))
        i += 1
        j += 1
    else:
        j += 1

print(f"Paquets appariés : {len(paired)}")

if len(paired) == 0:
    print("Aucun paquet détecté !")
    exit(1)

tx_t, rx_t, sz = zip(*paired)
rtt = [ (r - t)*1000 for t,r in zip(tx_t, rx_t) ]

# Débit instantané (bits entre deux RX consécutifs)
debit_t = []
debit_v = []
for k in range(1, len(rx_t)):
    dt = rx_t[k] - rx_t[k-1]
    if dt > 0:
        bps = sz[k-1] * 8 / dt
        debit_t.append(rx_t[k-1])
        debit_v.append(bps / 1e6)

# Graphiques 
fig = plt.figure(figsize=(14,11))

# Débit
plt.subplot(3,1,1)
plt.plot(debit_t, debit_v, 'o-', color='#2E86AB', linewidth=2, markersize=4, label='Débit instantané')
plt.axhline(y=np.mean(debit_v), color='red', linestyle='--', label=f'Moyenne = {np.mean(debit_v):.3f} Mbps')
plt.ylabel('Débit (Mbps)')
plt.title(f'Mode = {mode}')
plt.grid(alpha=0.3)
plt.legend()

# Latence
plt.subplot(3,1,2)
plt.plot(tx_t, rtt, 's-', color='#F77F00', markersize=4, label='RTT')
plt.axhline(y=np.mean(rtt), color='red', linestyle='--', label=f'Moyenne = {np.mean(rtt):.1f} ms')
plt.ylabel('Latence RTT (ms)')
plt.grid(alpha=0.3)
plt.legend()

# Perte
tx_total = len(tx_times)
rx_total = len(rx_times)
loss_rate = 100 * (tx_total - rx_total) / tx_total if tx_total > 0 else 0
plt.subplot(3,1,3)
plt.plot([tx_t[0], tx_t[-1]], [loss_rate, loss_rate], '^-', color='#D62828', linewidth=3, label=f'Taux de perte = {loss_rate:.1f} %')
plt.ylabel('Perte (%)')
plt.xlabel('Temps (s)')
plt.grid(alpha=0.3)
plt.legend()

plt.tight_layout()
plt.savefig(f'courbes_{mode}.png', dpi=300)
plt.show()

print(f"\nDébit moyen      : {np.mean(debit_v):.3f} Mbps")
print(f"Latence moyenne  : {np.mean(rtt):.1f} ms")
print(f"Taux de perte    : {loss_rate:.1f} %")
