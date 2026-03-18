# 🌦️ Station Météo Intelligente - Edge AI (STM32N6)

Ce projet utilise un réseau de neurones convolutionnel (1D-CNN) pour prédire la pluie en local sur une carte NUCLEO-N657X0.

## 🍃 Philosophie Green AI
* **Inférence Locale** : Pas de Cloud, donc moins de consommation d'énergie réseau.
* **Optimisation NPU** : Utilisation de l'accélérateur matériel Neural-ART.
* **Quantification** : Modèle en `int8` pour réduire l'empreinte mémoire.

## 📊 Benchmark des modèles
Nous avons comparé 4 architectures (MLP, RNN, CNN, Random Forest). Le 1D-CNN a été choisi pour son excellent rapport performance/efficience sur STM32.

![Matrices de confusion](model_training/comparaison.png)

## 🚀 Structure du dépôt
* `/Model_Training` : Notebook Colab et modèle TFLite quantifié.
* `/STM32_Project` : Code source C et configuration pour STM32CubeIDE (en cours).