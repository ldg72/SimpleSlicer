# Preparazione del modello

Il pannello **Model Transform**, sotto la viewport, modifica la geometria usata
sia dall'anteprima sia dallo slicing. Le trasformazioni funzionano su LWO, OBJ
e STL. Il file originale non viene modificato.

- **Position (mm)**: X/Y indicano il centro dell'ingombro rispetto al centro
  del piatto; Z indica la quota del punto più basso. Z = 0 appoggia il pezzo.
- **Rotation (deg)**: angoli applicati nell'ordine X, Y, Z. La scala agisce
  sugli assi del modello prima della rotazione.
- **Scale (%)**: 100 significa dimensioni importate. Con **Lock proportions**
  attivo, modificare un solo asse ridimensiona proporzionalmente anche gli altri.
  Disattivarlo permette scale indipendenti su X, Y, Z.
- **Apply**, oppure Invio in un campo, aggiorna il modello. Anche Slice applica
  i valori pendenti prima di esportare.
- **Center XY** azzera X e Y senza cambiare Z; **Drop to Bed** azzera solo Z.
- **Reset** ripristina posizione e rotazione a zero e scala al 100%, mantenendo
  la conversione delle unità di importazione.

Un nuovo file parte centrato e appoggiato al piano. Cambiare **Import Units**
mantiene posizione, rotazione e scala percentuale, ricalcolando dall'originale.
La conversione di unità resta riservata a LWO/OBJ; la scala percentuale vale
anche per STL. Auto-Fit conserva il precedente significato: dimensione massima
di importazione di 200 mm, prima delle trasformazioni manuali.

**Size** mostra le dimensioni finali dell'ingombro dopo la rotazione. Il messaggio
sottostante segnala l'uscita dal volume stampabile. Lo slicing non ridimensiona
più il modello di nascosto: si può tornare al pannello per correggerlo oppure
scegliere esplicitamente Slice Anyway. La verifica riguarda la geometria del
pezzo, non l'ingombro aggiuntivo di skirt, brim o supporti.

Il trascinamento con il sinistro e la rotella controllano solo la camera.
La camera ora resta riferita al piatto, così spostamenti e ridimensionamenti
del modello risultano visibili. Le modifiche non sono salvate tra sessioni.

## Esportazione e CuraEngine

La mesh visualizzata viene esportata in STL temporaneo con fattore 1, anche per
gli STL importati. Il comando usa `autoCenter=2`, `objectSink=0` e `posx`/`posy`
pari a metà delle dimensioni del piatto, in micrometri. Questi parametri seguono
la CLI legacy già usata dal progetto e assumono coordinate macchina con origine
XY nell'angolo del piatto. Vengono scritti dopo gli argomenti aggiuntivi per
preservare il posizionamento del pannello.

Il comportamento è stato verificato nei sorgenti upstream di CuraEngine 15.04:
[optimizedModel.h](https://github.com/Ultimaker/CuraEngine/blob/15.04/src/optimizedModel.h)
e [fffProcessor.h](https://github.com/Ultimaker/CuraEngine/blob/15.04/src/fffProcessor.h).
La build AmigaOS installata deve supportare la stessa CLI; la prova del G-code
generato va eseguita con quella build.

## Verifiche

Su macOS: `python3 tests/run_host_tests.py`. I test compilano il codice reale di
trasformazione e parsing/esportazione STL, adattando solo tipi Amiga e percorsi
degli header OpenGL. Usano UndefinedBehaviorSanitizer e verificano posizionamento,
rotazioni composte, scala, normali, preservazione dell'originale, limiti del
piatto e rilettura dell'STL esportato. Non avviano l'interfaccia MUI.

Su AmigaOS, compilare con `make`, quindi provare un modello di dimensioni note:
spostarlo in X/Y, ruotarlo di 90 gradi, scalarlo al 200%, sbloccare le proporzioni,
modificare un solo asse, usare Center XY, Drop to Bed e Reset. Verificare anche
valori non validi, uscita dal piatto, conversioni LWO e posizione nel G-code.
