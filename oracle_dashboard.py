import streamlit as st
import pandas as pd
import requests

# --- CONFIGURATION DES CANAUX ---
CH1_ID = "3339275"  # Météo Direct + Températures
CH2_ID = "3358311"  # Risque de pluie

st.set_page_config(page_title="L'Oracle Météo - STM32N6", page_icon="🌤️", layout="wide")

# --- BARRE LATÉRALE : PERFORMANCE & TECHNIQUE ---
st.sidebar.header("🎯 Spécifications IA")
st.sidebar.write("**Modèle :** CNN (Convolutional Neural Network)")
st.sidebar.write("**Optimisation :** Quantification int8 ")

st.sidebar.markdown("---")
if st.sidebar.button(' Actualiser les données'):
    st.rerun()

# --- ENTÊTE ---
st.title(" L'Oracle Météo : Intelligence Artificielle Embarquée")
st.write("Projet de déploiement Edge AI sur microcontrôleur STM32N6.")

# Section Soutenabilité (Argumentaire Oral)
with st.expander(" Pourquoi l'IA est-elle pertinente ici ? (Analyse de soutenabilité)"):
    st.write("""
    Contrairement au projet **Sinus** où une solution analytique simple est plus sobre numériquement, 
    la prédiction météo repose sur des relations **non-linéaires** complexes. 
    L'IA apporte ici une valeur ajoutée réelle en capturant des tendances que des calculs déterministes classiques 
    auraient du mal à modéliser sur un système aussi contraint.
    """)

# --- RÉCUPÉRATION DES DONNÉES ---
def get_data(channel_id):
    url = f"https://api.thingspeak.com/channels/{channel_id}/feeds.json?results=30"
    try:
        r = requests.get(url).json()
        return pd.DataFrame(r['feeds'])
    except:
        return None

df_temp = get_data(CH1_ID)
df_pluie = get_data(CH2_ID)

if df_temp is not None and not df_temp.empty:
    # --- 1. MÉTRIQUES EN DIRECT ---
    last_temp = df_temp['field1'].iloc[-1]
    last_press = df_temp['field2'].iloc[-1]
    last_hum = df_temp['field3'].iloc[-1]
    
    st.subheader("📍 Conditions Actuelles")
    col1, col2, col3 = st.columns(3)
    col1.metric("Température", f"{last_temp} °C")
    col2.metric("Pression Atmosphérique", f"{last_press} hPa")
    col3.metric("Humidité Relative", f"{last_hum} %")

    st.markdown("---")

    # --- 2. EXPLICATION IA ---
    st.info("💡 **Le concept :** L'IA analyse les tendances des **24 dernières heures** (Fenêtre Glissante) "
            "pour générer ces prédictions sur 5 jours.")

    # --- 3. PRÉVISIONS IA (J+1 à J+5) EN MÉTRIQUES ---
    st.subheader("📈 Prévisions de Température")
    cols_t = st.columns(5)
    champs_t = ['field4', 'field5', 'field6', 'field7', 'field8']
    jours = ['J+1', 'J+2', 'J+3', 'J+4', 'J+5']
    
    for col, jour, champ in zip(cols_t, jours, champs_t):
        val = df_temp[champ].iloc[-1]
        col.metric(jour, f"{val} °C")

    st.markdown("---")

    if df_pluie is not None and not df_pluie.empty:
        st.subheader("☔ Probabilité de Précipitations")
        cols_p = st.columns(5)
        champs_p = ['field1', 'field2', 'field3', 'field4', 'field5']
        
        for col, jour, champ in zip(cols_p, jours, champs_p):
            val = df_pluie[champ].iloc[-1]
            col.metric(jour, f"{val} %")
    else:
        st.warning("Données de pluie non disponibles.")

else:
    st.error("Impossible de joindre le serveur ThingSpeak ou aucune donnée trouvée.")

st.markdown("---")
st.caption("Université Savoie Mont Blanc - Module Systèmes Embarqués & IA - Projet STM32N6")