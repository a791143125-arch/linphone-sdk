import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

table_file = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja/run_0/metrics.ods"
df = pd.read_excel(table_file)
print(df)



df_real_noise = df[df["noise"] == "real (echo 4)"]

df_rnnoise_real_noise = df_real_noise[df_real_noise["model"] == "rnnoise"]
df_webrtcns_real_noise = df_real_noise[df_real_noise["model"] == "mswebrtcns"]
df_rnnoise_white_noise = df[df["noise"] == "white noise"]

print(f"RNNoise with white noise:\n{df_rnnoise_white_noise}")
print(f"RNNoise with real noise:\n{df_rnnoise_real_noise}")
print(f"WebRTCNS with real noise:\n{df_webrtcns_real_noise}")

fig = make_subplots(rows=1, cols=2, shared_xaxes=True)
RNNoise_col = "#1f77b4"
WebRTCNS_col = "#ff7f0e"
line_width = 2
marker_size = 10
# plot sim_talk = f(SNR)
fig.add_trace(go.Scatter(
    x=df_webrtcns_real_noise["SNR (dB)"],
    y=df_webrtcns_real_noise["similarity"],
    mode='lines+markers',
    line={'width': line_width, "color": WebRTCNS_col},
    marker={"symbol": "circle-open", "size": marker_size},
    name=f"WebRTC NS, real noise"),
    row=1, col=1)
fig.add_trace(go.Scatter(
    x=df_rnnoise_real_noise["SNR (dB)"],
    y=df_rnnoise_real_noise["similarity"],
    mode='lines+markers',
    line={'width': line_width, "color": RNNoise_col},
    marker={"symbol": "circle-open", "size": marker_size},
    name=f"RNNoise, real noise"),
    row=1, col=1)
fig.add_trace(go.Scatter(
    x=df_rnnoise_white_noise["SNR (dB)"],
    y=df_rnnoise_white_noise["similarity"],
    mode='lines+markers',
    line={'width': line_width, "color": RNNoise_col},
    marker={"symbol": "star-open", "size": marker_size},
    name=f"RNNoise, white noise"),
    row=1, col=1)
fig.update_yaxes(title_text='Similarity in speech parts', range=[0.8, 1.], row=1, col=1)

# plot energy_silence = f(SNR)
fig.add_trace(go.Scatter(
    x=df_webrtcns_real_noise["SNR (dB)"],
    y=df_webrtcns_real_noise["energy in silence"],
    mode='lines+markers',
    line={'width': line_width, "color": WebRTCNS_col},
    marker={"symbol": "circle-open", "size": 10},
    name=f"WebRTC NS, real noise",
    showlegend=False,),
    row=1, col=2)
fig.add_trace(go.Scatter(
    x=df_rnnoise_real_noise["SNR (dB)"],
    y=df_rnnoise_real_noise["energy in silence"],
    mode='lines+markers',
    line={'width': line_width, "color": RNNoise_col},
    marker={"symbol": "circle-open", "size": 10},
    name=f"RNNoise, real noise",
    showlegend=False,),
    row=1, col=2)
fig.add_trace(go.Scatter(
    x=df_rnnoise_white_noise["SNR (dB)"],
    y=df_rnnoise_white_noise["energy in silence"],
    mode='lines+markers',
    line={'width': line_width, "color": RNNoise_col},
    marker={"symbol": "star-open", "size": 10},
    name=f"RNNoise, white noise",
    showlegend=False,),
    row=1, col=2)
fig.update_yaxes(title_text='Remaining energy in silence parts', range=[0, 2], row=1, col=2)


fig.update_layout(title="Noise suppressor performances")
fig.update_xaxes(title_text='SNR in dB')
fig.show()