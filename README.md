# Classificador de sons no ESP32 (Aulas 19 e 20)

Classificação offline de sons com ESP32 + INMP441 e um modelo CNN INT8 treinado no Edge Impulse.

Classes: `assobio`, `batida_mesa`, `palma`, `ruido_ambiente`

- `dataset/`: WAVs mono, 16 kHz, PCM16, ~2 s (cerca de 20 por classe)
- `inferencia_esp32/`: firmware Arduino que roda a inferência no ESP32
- [PASSO_A_PASSO.md](PASSO_A_PASSO.md): guia completo, do dataset ao deploy
