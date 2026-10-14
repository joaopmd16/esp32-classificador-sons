# Aulas 19 e 20: passo a passo

Fluxo: **INMP441 → ESP32 (I²S) → WAV → Edge Impulse (MFE + CNN) → INT8 → inferência offline no ESP32**

---

## Etapa 1: Dataset ✅ (já feito)

Verifiquei a pasta `dataset/`:

| Classe | Arquivos | Formato |
|---|---|---|
| assobio | 20 | mono, 16 kHz, PCM16, ~2 s |
| batida_mesa | 20 | mono, 16 kHz, PCM16, ~2 s |
| palma | **21** | mono, 16 kHz, PCM16, ~2 s |
| ruido_ambiente | 20 | mono, 16 kHz, PCM16, 2 s |

Pontos de atenção:
- **palma tem 2 arquivos `palma_001`** (21-15-42 e 21-16-29). O primeiro parece ser uma gravação de teste. Pode apagar, ou deixar (não atrapalha).
- `palma_004` está bem fraco (pico 6863, enquanto as outras palmas chegam perto de 32767). Ouça esse arquivo: se não tiver palma, apague ou regrave.
- Muitas batidas e palmas **saturam** (pico 32767). Funciona, mas se for regravar, afaste um pouco o microfone.

---

## Etapa 2: Criar o projeto no Edge Impulse

1. Acesse https://studio.edgeimpulse.com e faça login.
2. **Create new project** → dê um nome (ex.: `sons_esp32`). Esse nome vai virar o nome da biblioteca (`sons_esp32_inferencing`).
3. Em **Dashboard**, escolha o tipo **Audio**.

## Etapa 3: Upload (uma vez por classe)

**Data acquisition → Upload data** (ícone de upload):
1. **Select a folder** → escolha `dataset/assobio`.
2. **Upload into category:** *Automatically split between training and testing* (80/20).
3. **Label:** *Enter label* → digite `assobio` (exatamente igual ao nome da pasta).
4. **Upload data** → confira o log: "20 files, 0 failed".
5. Repita para `batida_mesa`, `palma` e `ruido_ambiente`.

✔ Confira: 4 rótulos aparecem, há amostras em **Training** e em **Test**.

## Etapa 4: Create impulse

**Impulse design → Create impulse**:
- **Time series data:** Window size **2000 ms**, Window increase 500 ms, Frequency **16000 Hz**, *Zero-pad data* marcado.
- **Add processing block:** **Audio (MFE)**.
- **Add learning block:** **Classification**.
- ✔ Output features: **4** (assobio, batida_mesa, palma, ruido_ambiente).
- **Save Impulse**.

## Etapa 5: MFE

**Impulse design → MFE**:
- Frame length **0.02**, Frame stride **0.01**, Filter number **40**, FFT length **256**.
- Low frequency 0, High frequency 0 (= até 8 kHz), Noise floor −52 dB (padrão).
- **Save parameters** → aba **Generate features** → **Generate features**.
- Aguarde **Job completed (success)**. No Feature explorer, as 4 classes devem formar grupos.

## Etapa 6: Treinar a CNN

**Impulse design → Classifier**:
- Number of training cycles **100**, Learning rate **0.005**, Training processor **CPU**, arquitetura padrão.
- ✔ Antes de treinar: a camada de saída deve ter **4 classes**.
- **Start training**.

Anote: acurácia, loss e a **matriz de confusão** (validação). Na aula, a palma foi a classe mais confundida (50%) e o ruído ficou em 100%.

## Etapa 7: Teste reservado

**Model testing → Classify all**. Anote a acurácia e a matriz de confusão (versão Float32).
> Com poucas amostras (~17 no teste), 100% **não** garante que vai funcionar em outro ambiente.

## Etapa 8: Exportar para Arduino

**Deployment**:
- Target: **Arduino library**
- Engine: **EON Compiler**
- Optimization: **Quantized (int8)**
- **Build** → baixa um `.zip` (ex.: `ei-sons_esp32-arduino-1.0.1.zip`).

## Etapa 9: Arduino IDE

1. Se tiver uma versão antiga da biblioteca, apague a pasta dela em `Documentos\Arduino\libraries\`.
2. **Sketch → Include Library → Add .ZIP Library...** → escolha o zip.
3. Placa: **Tools → Board → esp32 → ESP32 Dev Module** (instale o pacote *esp32 by Espressif* no Boards Manager, se precisar).
4. Abra `inferencia_esp32/inferencia_esp32.ino` (está nesta pasta).
5. Troque a linha `#include <SEU_PROJETO_inferencing.h>` pelo nome da sua biblioteca (ex.: `#include <sons_esp32_inferencing.h>`).
6. Se o sketch que gravou os WAVs convertia o áudio com um deslocamento diferente de `>> 14`, ajuste `SHIFT_PCM16` para o mesmo valor. **O pré-processamento precisa ser igual no treino e na inferência.**
7. **Upload** → abra o **Serial Monitor a 115200**.

> A primeira compilação demora bastante (vários minutos). É normal.

Saída esperada:
```
>>> Gravando 2 s... faca o som agora
DSP: 117 ms | Inferencia: 26 ms
  assobio: 1.2%
  batida_mesa: 0.8%
  palma: 0.7%
  ruido_ambiente: 97.3%
==> CLASSE: ruido_ambiente (97.3%)
```

### Se der problema
| Sintoma | Causa provável |
|---|---|
| `SEU_PROJETO_inferencing.h: No such file` | Nome do include errado. Veja o nome em Sketch → Include Library |
| Tudo sempre `ruido_ambiente`, mesmo batendo palma | Microfone lendo zero: confira os fios ou troque para `I2S_CHANNEL_FMT_ONLY_RIGHT` |
| Classe sempre a mesma com score estranho | `SHIFT_PCM16` diferente do usado na gravação (volume diferente) |
| Erro de compilação no `driver/i2s.h` | Core ESP32 3.x mostra aviso de "deprecated": pode ignorar. Se der erro, use o core 2.0.17 |

## Etapa 10: Atividade (o que entregar)

- [ ] ≥ 20 WAVs por classe, 4 classes desde o início (incluindo `ruido_ambiente`)
- [ ] Upload rotulado com conjunto de teste reservado
- [ ] Prints: matriz de confusão (validação) e Model testing
- [ ] Export Arduino INT8 compilado e rodando no ESP32
- [ ] Prints do Serial Monitor com **acertos** e **falsos positivos** (ex.: falar perto do mic, bater no teclado, palma fraca) para discussão
