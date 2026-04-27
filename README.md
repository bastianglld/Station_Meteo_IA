L'Oracle Météo : Edge AI sur STM32N6

Ce projet consiste en la conception et le déploiement d'une station météorologique intelligente capable de réaliser des prévisions locales à 5 jours (J+1 à J+5) directement sur microcontrôleur. Ce travail a été réalisé dans le cadre du module "Systèmes Embarqués et Intelligence Artificielle au plus proche des Capteurs" de l'Université Savoie Mont Blanc.

------------> Présentation du Projet <------------


L'objectif est de dépasser la simple mesure de capteurs pour offrir une capacité de prédiction autonome ("Edge AI"). Le système utilise un réseau de neurones entraîné sur des données historiques pour estimer l'évolution de la température et le risque de précipitations à partir des conditions actuelles.

Architecture Technique
1. Matériel (Hardware)

Microcontrôleur : STM32N657 (Cortex-M33 cadencé à 160 MHz).

Capteurs : HTS221 (Température/Humidité) et LPS22HH (Pression) reliés en I2C.

Transmission : Liaison série (UART) vers une passerelle (Gateway) PC.

2. Intelligence Artificielle & Inférence

Modèle : Réseau de neurones convolutif (CNN).
Entraînement : Réalisé hors-ligne sur PC (TensorFlow/Keras) à partir de la base de données Meteostat.
Optimisation : Quantification int8 pour réduire l'empreinte mémoire et accélérer les calculs.
Exécution : Le modèle est exécuté par le CPU (Cortex-M33) de la carte. Bien que la carte dispose d'un NPU, cette implémentation utilise les unités de calcul classiques pour valider la robustesse du modèle en mode processeur standard.

3. Pipeline IoT

Gateway Python : Un script récupère les prédictions via le port série et les pousse vers l'API ThingSpeak.
Dashboard : Une interface Streamlit publique permet de consulter les données et les prévisions en temps réel depuis n'importe quel navigateur.



------------> Structure du Dépôt <------------

/firmware : Code source C pour STM32CubeIDE (logique d'acquisition et inférence X-CUBE-AI).
/models : Fichier .tflite quantifié et métriques d'entraînement (Erreur RMS).
/scripts : Script Python pont_thingspeak.py pour la liaison série-Cloud.
/dashboard : Code source de l'interface Web Streamlit.

Analyse de Soutenabilité & Sobriété
Conformément aux objectifs du module, une réflexion critique a été menée sur l'usage de l'IA:

IA vs Analytique : Contrairement au projet sinus où l'IA est moins efficace qu'une formule, la météo justifie l'usage d'un réseau de neurones pour modéliser des relations non-linéaires complexes.

Frugalité : L'utilisation de la quantification int8 permet de faire tourner le modèle sur le CPU avec une latence très faible, évitant ainsi le recours à des processeurs plus gourmands en énergie.

Installation Rapide
Lancer le pont série :

Bash
python3 scripts/pont_thingspeak.py
Lancer le Dashboard (Local) :

Bash
cd dashboard
pip install -r requirements.txt
streamlit run oracle_dashboard.py
Projet réalisé par l'équipe Ramazan Tanisik, Messaoud Zanouda, Bastian Guilloud - USMB 2026
