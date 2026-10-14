/*
 * Aulas 19 e 20 - Classificação de sons no ESP32 (offline)
 * Classes: assobio, batida_mesa, palma, ruido_ambiente
 *
 * Hardware: ESP32 DevKitC V4 + INMP441
 *   INMP441 VDD -> 3V3     GND -> GND     L/R -> GND (canal esquerdo)
 *   SCK  (BCLK) -> GPIO17
 *   WS   (LRCLK)-> GPIO16
 *   SD   (DOUT) -> GPIO33
 *
 * ANTES DE COMPILAR: troque o include abaixo pelo nome da SUA biblioteca
 * exportada do Edge Impulse (Sketch > Include Library mostra o nome exato).
 */
#include <Gorila-project-1_inferencing.h>
#include <driver/i2s.h>

#define I2S_PORT   I2S_NUM_0
#define PIN_BCLK   17
#define PIN_WS     16
#define PIN_SD     33

// O INMP441 entrega 24 bits dentro de palavras de 32 bits.
// Deslocar 16 bits = valor "puro"; 14 bits = ganho x4.
// Use o MESMO valor do sketch que gravou os WAVs do dataset.
#define SHIFT_PCM16 14

#define LIMIAR_CONFIANCA 0.60f

static int16_t *amostras = nullptr;   // 2 s * 16 kHz = 32.000 amostras
static int32_t bufI2S[512];

// ---------- 1. Inicializar I2S ----------
static void i2s_init() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = EI_CLASSIFIER_FREQUENCY;               // 16000
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;          // se vier só zero, teste ONLY_RIGHT
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 512;
  cfg.use_apll = false;

  i2s_pin_config_t pinos = {};
  pinos.mck_io_num = I2S_PIN_NO_CHANGE;
  pinos.bck_io_num = PIN_BCLK;
  pinos.ws_io_num = PIN_WS;
  pinos.data_out_num = I2S_PIN_NO_CHANGE;
  pinos.data_in_num = PIN_SD;

  i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  i2s_set_pin(I2S_PORT, &pinos);
  i2s_zero_dma_buffer(I2S_PORT);
}

// ---------- 2 e 3. Capturar 32.000 amostras e converter para PCM16 ----------
static void capturar_janela() {
  size_t total = 0;
  while (total < EI_CLASSIFIER_RAW_SAMPLE_COUNT) {
    size_t lidos = 0;
    i2s_read(I2S_PORT, bufI2S, sizeof(bufI2S), &lidos, portMAX_DELAY);
    size_t n = lidos / sizeof(int32_t);
    for (size_t i = 0; i < n && total < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
      int32_t v = bufI2S[i] >> SHIFT_PCM16;
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      amostras[total++] = (int16_t)v;
    }
  }
}

// ---------- 4. Callback usado pelo signal_t ----------
static int ler_sinal(size_t offset, size_t length, float *out_ptr) {
  numpy::int16_to_float(&amostras[offset], out_ptr, length);
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Classificador de sons - Edge Impulse no ESP32");

  amostras = (int16_t *)malloc(EI_CLASSIFIER_RAW_SAMPLE_COUNT * sizeof(int16_t));
  if (!amostras) {
    Serial.println("ERRO: sem memoria para o buffer de audio");
    while (true) delay(1000);
  }

  i2s_init();

  // Descarta ~0,5 s iniciais (o INMP441 leva um tempo para estabilizar)
  for (int i = 0; i < 16; i++) {
    size_t lidos;
    i2s_read(I2S_PORT, bufI2S, sizeof(bufI2S), &lidos, portMAX_DELAY);
  }

  Serial.printf("Janela: %d ms | Amostras: %d | Classes: %d\n",
                EI_CLASSIFIER_INTERVAL_MS * EI_CLASSIFIER_RAW_SAMPLE_COUNT,
                EI_CLASSIFIER_RAW_SAMPLE_COUNT, EI_CLASSIFIER_LABEL_COUNT);
}

void loop() {
  Serial.println("\n>>> Gravando 2 s... faca o som agora");
  capturar_janela();

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &ler_sinal;

  // ---------- 5. Executar run_classifier() ----------
  ei_impulse_result_t resultado = { 0 };
  EI_IMPULSE_ERROR err = run_classifier(&signal, &resultado, false);
  if (err != EI_IMPULSE_OK) {
    Serial.printf("ERRO run_classifier: %d\n", err);
    return;
  }

  // ---------- 6. Imprimir probabilidades e identificar a classe ----------
  Serial.printf("DSP: %d ms | Inferencia: %d ms\n",
                resultado.timing.dsp, resultado.timing.classification);

  size_t melhor = 0;
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.printf("  %s: %.1f%%\n", resultado.classification[i].label,
                  100.0f * resultado.classification[i].value);
    if (resultado.classification[i].value > resultado.classification[melhor].value) melhor = i;
  }

  if (resultado.classification[melhor].value >= LIMIAR_CONFIANCA) {
    Serial.printf("==> CLASSE: %s (%.1f%%)\n", resultado.classification[melhor].label,
                  100.0f * resultado.classification[melhor].value);
  } else {
    Serial.println("==> Incerto (abaixo do limiar)");
  }

  delay(500);
}
