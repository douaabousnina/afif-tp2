import matplotlib.pyplot as plt
import subprocess
import re
import numpy as np
from matplotlib.gridspec import GridSpec

print("=" * 60)
print("   ANALYSE DE L'IMPACT DU NOMBRE DE NŒUDS")
print("=" * 60)

# Menu principal
print("\n=== Type de variation ===")
print("1. Varier le nombre de nœuds WiFi (nWifi)")
print("2. Varier le nombre de nœuds CSMA (nCsma)")
print("3. Varier les deux simultanément")
print("=" * 60)

while True:
    type_variation = input("Choisissez le type de variation (1-3) : ").strip()
    if type_variation in ["1", "2", "3"]:
        break
    print(" Choix invalide ! Entrez 1, 2 ou 3.")

# Choix du mode de charge
print("\n=== Mode de charge ===")
print("1. low    (faible charge)")
print("2. medium (charge moyenne)")
print("3. high   (charge élevée)")
print("=" * 60)

while True:
    choix_mode = input("Choisissez le mode (1-3) : ").strip()
    if choix_mode == "1":
        mode = "low"
        break
    elif choix_mode == "2":
        mode = "medium"
        break
    elif choix_mode == "3":
        mode = "high"
        break
    print(" Choix invalide ! Entrez 1, 2 ou 3.")

print(f"\n Mode sélectionné : {mode}")

# Configuration automatique des variations (de 3 à 30 nœuds)
variation_range = [3, 6, 9, 12, 15, 18, 21, 24, 27, 30]

if type_variation == "1":
    print("\n=== Configuration WiFi ===")
    print(f"Variation automatique de nWifi : {variation_range}")
    nwifi_list = variation_range.copy()
    ncsma_list = [3] * len(nwifi_list) 
    param_name = "nWifi"
    
elif type_variation == "2":
    print("\n=== Configuration CSMA ===")
    print(f"Variation automatique de nCsma : {variation_range}")
    ncsma_list = variation_range.copy()
    nwifi_list = [3] * len(ncsma_list)  
    param_name = "nCsma"
    
else: 
    print("\n=== Configuration WiFi et CSMA ===")
    print(f"Variation automatique : {variation_range}")
    nwifi_list = variation_range.copy()
    ncsma_list = variation_range.copy()
    param_name = "nWifi & nCsma"

print(f"\n Lancement de {len(nwifi_list)} simulations...\n")

# Stockage des résultats
resultats = {
    'debit_moyen': [],
    'latence_moyenne': [],
    'taux_perte': [],
    'param_values': []
}

# Exécution des simulations
for idx, (nwifi, ncsma) in enumerate(zip(nwifi_list, ncsma_list), 1):
    print(f"\n{'='*60}")
    print(f"  Simulation {idx}/{len(nwifi_list)} : nWifi={nwifi}, nCsma={ncsma}")
    print(f"{'='*60}")
    
    cmd = f"./ns3 run \"scratch/third3 --mode={mode} --nWifi={nwifi} --nCsma={ncsma}\" 2>&1 | tee simulation_output.log"
    subprocess.run(cmd, shell=True)
    
    # Extraction des données
    tx_times = []
    rx_times = []
    sizes = []
    
    with open("simulation_output.log", "r") as f:
        lines = f.readlines()
    
    for line in lines:
        m_tx = re.search(r'At time \+([\d\.]+)s client sent (\d+) bytes', line)
        m_rx = re.search(r'At time \+([\d\.]+)s client received (\d+) bytes', line)
        if m_tx:
            tx_times.append(float(m_tx.group(1)))
            sizes.append(int(m_tx.group(2)))
        if m_rx:
            rx_times.append(float(m_rx.group(1)))
    
    # Appariement
    rx_times_sorted = sorted(rx_times)
    tx_times_sorted = sorted(tx_times)
    
    paired = []
    i = j = 0
    while i < len(tx_times_sorted) and j < len(rx_times_sorted):
        if tx_times_sorted[i] < rx_times_sorted[j]:
            paired.append((tx_times_sorted[i], rx_times_sorted[j], 
                          sizes[i] if i < len(sizes) else 1024))
            i += 1
            j += 1
        else:
            j += 1
    
    if len(paired) > 0:
        tx_t, rx_t, sz = zip(*paired)
        rtt = [(r - t) * 1000 for t, r in zip(tx_t, rx_t)]
        
        # Calcul du débit
        debit_vals = []
        for k in range(1, len(rx_t)):
            dt = rx_t[k] - rx_t[k-1]
            if dt > 0:
                bps = sz[k-1] * 8 / dt
                debit_vals.append(bps / 1e6)
        
        # Taux de perte
        tx_total = len(tx_times)
        rx_total = len(rx_times)
        loss_rate = 100 * (tx_total - rx_total) / tx_total if tx_total > 0 else 0
        
        # Stockage
        resultats['debit_moyen'].append(np.mean(debit_vals) if len(debit_vals) > 0 else 0)
        resultats['latence_moyenne'].append(np.mean(rtt))
        resultats['taux_perte'].append(loss_rate)
        
        if type_variation == "1":
            resultats['param_values'].append(nwifi)
        elif type_variation == "2":
            resultats['param_values'].append(ncsma)
        else:
            resultats['param_values'].append(f"{nwifi},{ncsma}")
        
        print(f"  ✓ Débit: {resultats['debit_moyen'][-1]:.3f} Mbps")
        print(f"  ✓ Latence: {resultats['latence_moyenne'][-1]:.2f} ms")
        print(f"  ✓ Perte: {resultats['taux_perte'][-1]:.1f} %")
    else:
        print("   Aucun paquet détecté pour cette configuration")
        resultats['debit_moyen'].append(0)
        resultats['latence_moyenne'].append(0)
        resultats['taux_perte'].append(100)
        
        if type_variation == "1":
            resultats['param_values'].append(nwifi)
        elif type_variation == "2":
            resultats['param_values'].append(ncsma)
        else:
            resultats['param_values'].append(f"{nwifi},{ncsma}")

# Création des graphiques
print("\n\n Génération des graphiques...")

fig = plt.figure(figsize=(16, 10))
gs = GridSpec(2, 2, figure=fig, hspace=0.3, wspace=0.3)

x_values = resultats['param_values']
if type_variation != "3":
    x_values = [int(x) for x in x_values]

# 1. Débit moyen
ax1 = fig.add_subplot(gs[0, 0])
if type_variation == "3":
    ax1.plot(range(len(x_values)), resultats['debit_moyen'], 'o-', 
             color='#2E86AB', linewidth=2.5, markersize=8)
    ax1.set_xticks(range(len(x_values)))
    ax1.set_xticklabels(x_values, rotation=45, ha='right')
else:
    ax1.plot(x_values, resultats['debit_moyen'], 'o-', 
             color='#2E86AB', linewidth=2.5, markersize=8)
ax1.set_ylabel('Débit moyen (Mbps)', fontsize=11, fontweight='bold')
ax1.set_xlabel(param_name, fontsize=11, fontweight='bold')
ax1.set_title(f'Impact de {param_name} sur le Débit - Mode {mode}', 
              fontsize=12, fontweight='bold')
ax1.grid(alpha=0.3, linestyle='--')
ax1.set_facecolor('#f8f9fa')

# 2. Latence moyenne
ax2 = fig.add_subplot(gs[0, 1])
if type_variation == "3":
    ax2.plot(range(len(x_values)), resultats['latence_moyenne'], 's-', 
             color='#F77F00', linewidth=2.5, markersize=8)
    ax2.set_xticks(range(len(x_values)))
    ax2.set_xticklabels(x_values, rotation=45, ha='right')
else:
    ax2.plot(x_values, resultats['latence_moyenne'], 's-', 
             color='#F77F00', linewidth=2.5, markersize=8)
ax2.set_ylabel('Latence moyenne (ms)', fontsize=11, fontweight='bold')
ax2.set_xlabel(param_name, fontsize=11, fontweight='bold')
ax2.set_title(f'Impact de {param_name} sur la Latence - Mode {mode}', 
              fontsize=12, fontweight='bold')
ax2.grid(alpha=0.3, linestyle='--')
ax2.set_facecolor('#f8f9fa')

# 3. Taux de perte
ax3 = fig.add_subplot(gs[1, :])  
if type_variation == "3":
    ax3.plot(range(len(x_values)), resultats['taux_perte'], '^-', 
             color='#D62828', linewidth=2.5, markersize=8)
    ax3.set_xticks(range(len(x_values)))
    ax3.set_xticklabels(x_values, rotation=45, ha='right')
else:
    ax3.plot(x_values, resultats['taux_perte'], '^-', 
             color='#D62828', linewidth=2.5, markersize=8)
ax3.set_ylabel('Taux de perte (%)', fontsize=11, fontweight='bold')
ax3.set_xlabel(param_name, fontsize=11, fontweight='bold')
ax3.set_title(f'Impact de {param_name} sur le Taux de Perte - Mode {mode}', 
              fontsize=12, fontweight='bold')
ax3.grid(alpha=0.3, linestyle='--')
ax3.set_facecolor('#f8f9fa')

filename = f'impact_{param_name.replace(" ", "_").replace("&", "et")}_{mode}.png'
plt.savefig(filename, dpi=300, bbox_inches='tight')
print(f" Graphiques sauvegardés : {filename}")

plt.show()

# Résumé des résultats
print("\n" + "="*60)
print("  RÉSUMÉ DES RÉSULTATS")
print("="*60)
print(f"{'Configuration':<20} {'Débit (Mbps)':<15} {'Latence (ms)':<15} {'Perte (%)'}")
print("-"*60)
for i in range(len(resultats['param_values'])):
    config = resultats['param_values'][i]
    if type_variation == "1":
        config_str = f"nWifi={config}"
    elif type_variation == "2":
        config_str = f"nCsma={config}"
    else:
        config_str = f"W={config.split(',')[0]},C={config.split(',')[1]}"
    
    print(f"{config_str:<20} {resultats['debit_moyen'][i]:<15.3f} "
          f"{resultats['latence_moyenne'][i]:<15.2f} "
          f"{resultats['taux_perte'][i]:<12.1f}")
print("="*60)
