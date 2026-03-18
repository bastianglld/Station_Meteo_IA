Rapport Technique : Station Météo Intelligente & Responsable
Équipe : Bastian(ESET),  Ramazan(TRI),  Messaoud(TRI)
Sujet : Développement et déploiement d'un modèle de prédiction de pluie sur cible embarquée.

1. Introduction et Objectifs
Dans le cadre de ce TP, notre équipe a eu pour mission de concevoir un système capable de prédire la probabilité de pluie à T+1 heure en se basant sur un historique de 24 heures de données météorologiques (Température, Humidité, Pression).
L'enjeu principal était de passer d'un modèle de recherche (développé sous Google Colab) à une implémentation réelle sur une carte NUCLEO-N657X0, tout en respectant des contraintes de sobriété numérique (Green AI) et d'efficacité matérielle.

2. Phase de Recherche : Benchmarking sur Colab
Avant de choisir notre modèle final, nous avons mené une étude comparative rigoureuse sur 4 architectures différentes. Ce "benchmark" nous a permis d'évaluer le compromis entre précision et complexité.
Résultats des modèles testés :
MLP (Multi-Layer Perceptron) : Notre "baseline". Bien que rapide, il ne parvient pas à capturer la dynamique temporelle et tend à prédire une classe majoritaire (Sec), manquant ainsi les épisodes de pluie.
RNN (LSTM) : Très efficace pour les séries temporelles, mais sa structure récurrente est gourmande en mémoire RAM et plus complexe à accélérer nativement.
Random Forest : Excellentes performances statistiques, mais ce modèle n'est pas optimisé pour le hardware spécifique (NPU) de notre carte.
1D-CNN (Choix Final) : Ce modèle a montré une capacité supérieure à détecter des motifs (comme une chute de pression) sur la fenêtre de 24h.

3. Choix Technologiques et Optimisation
Le choix du 1D-CNN a été dicté par l'architecture de la STM32N6.
Pourquoi le 1D-CNN ?
Hardware Acceleration : L'unité de calcul Neural-ART (NPU) de la carte est spécifiquement conçue pour accélérer les opérations de convolution.
Efficacité Énergétique : En déportant les calculs sur le NPU plutôt que sur le CPU (Cortex-M33), nous réduisons drastiquement la consommation par inférence.
Quantification INT8 : Nous avons converti notre modèle en format int8. Cela divise par 4 l'occupation mémoire et réduit les cycles de calcul, sans perte significative de précision.

4. Architecture de la Solution Embarquée
Sur la cible STM32, notre code (en cours d'intégration finale) s'articule autour de trois piliers :
Acquisition (Drivers) : Lecture des capteurs HTS221 (Temp/Hum) et LPS22HH (Pression) via le bus I2C2. Nous avons développé des fonctions platform_read robustes pour garantir la fiabilité des données d'entrée.
Traitement (X-CUBE-AI) : Utilisation de l'extension de ST pour mapper le modèle .tflite sur le NPU.
Gestion de l'énergie : Le système est conçu pour rester en Stop Mode la majorité du temps, ne se réveillant qu'une fois par heure pour une mesure et une inférence éclair.

5. Conclusion et Philosophie Green AI
Notre approche démontre qu'il est possible de créer une IA performante tout en étant éco-responsable. En privilégiant l'Edge AI (traitement local) plutôt que le Cloud, notre équipe a réduit l'empreinte carbone liée aux communications réseau.
L'utilisation combinée du 1D-CNN, de la quantification et de l'accélération matérielle fait de cette station météo un système autonome, sobre et intelligent, prêt pour une exploitation durable.
