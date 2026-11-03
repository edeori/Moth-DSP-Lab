# Moth DSP Lab — Fejlesztési specifikáció (v0.1)

> **Célközönség:** Codex / Claude Code fejlesztő agent.\
> **Dokumentum jellege:** implementáció-orientált product + architecture spec.\
> **Elsődleges cél:** meglévő audio plugin algoritmikus finomítása referencia alapján.\
> **AI az első verzióban:** Codex vagy Claude integráció; a lokális LLM későbbi, cserélhető opció.\
> **Nem cél:** neural capture, neurális amp-klón vagy pusztán IR-alapú modell létrehozása.

## 1. Termékvízió

A Moth DSP Lab egy cross-platform (elsőként macOS, később Windows/Linux) asztali audio DSP mérő-, összehasonlító és fejlesztéstámogató alkalmazás. Egységes módon hasonlít össze:

- fizikai erősítőt/pedált/audio hardvert és VST3/AU plugint;
- két tetszőleges plugint;
- pluginláncokat;
- azonos plugin különböző verzióit;
- előre rögzített referencia WAV-fájlokat.

A referencia és a jelölt (candidate) ugyanazt az ismert tesztjelet kapja. A rendszer kalibrál, mér, összehasonlít, diagnosztikai adatot készít, és opcionális AI-agent segítségével fejlesztési javaslatokat, célzott új mérési tervet és validálható DSP-változtatásokat állít elő.

**Alapelv:** az audio mérőmotor, az optimalizáló és az alapvető UI AI nélkül, internet nélkül is működjön. Internet csak a választott Codex/Claude kapcsolathoz szükséges; a mérési adatok helyben maradnak, amíg a felhasználó kifejezetten nem küldi el őket.

## 2. Prioritások és működési módok

### P0 — Refine Existing Plugin (elsődleges)

1. Töltsön be tetszőleges VST3 plugint, macOS-en lehetőség szerint AU-t is.
2. A felhasználó válasszon referenciát: hardver, másik plugin, pluginlánc, WAV.
3. A program a host által közzétett pluginparamétereket olvassa és állítsa, mentse a plug-in state-et/presetet.
4. Opcionálisan hozzárendelhető a plugin forrásprojektje, build parancsa és belső paramétermanifestje.
5. A rendszer mérjen több jelszinten, potméterállásban és jelalakkal.
6. Készüljön objektív differenciaanalízis, majd opcionális AI diagnózis és célzott fejlesztési javaslat.
7. A változások újraméréssel és regressziós tesztekkel igazolandók.

### P1 — Universal A/B Analyzer

Két tetszőleges AudioTarget összehasonlítása és reprodukálható riport, forráskód-hozzáférés nélkül is.

### P2 — Synthesize DSP Model

Új parametrikus, explicit DSP-gráf javaslata mérésekből, opcionálisan kapcsolási rajzból/SPICE netlistből. A generált modell ne neural capture legyen: szűrők, dinamikus rendszerek, nemlineáris fokozatok, feedback és más auditálható blokkok használhatók. Az automatikus generálás kutatási jellegű, kezdetben javaslatot adjon, ne ígérjen teljesen helyes kódot.

## 3. UX — fő munkafolyamat

### 3.1 Projekt létrehozása

- **Project name**, sample rate, block size, csatornaszám, mérési profil.
- **Reference source A:** Hardware / Plugin / Chain / Recorded file.
- **Candidate source B:** Hardware / Plugin / Chain / Recorded file.
- Plugin scan, kiválasztás, GUI/paraméterlista, preset, bypass.
- Audio device, kimeneti és bemeneti csatornák, biztonságos gain beállítás.
- Menthető session és teljes routingkonfiguráció.

### 3.2 Potméterállások

- Pluginparaméterek automatikus olvasása a host API-ból (ha támogatott).
- A felhasználó manuálisan is létrehozhat fizikai potmétereket (név, min/max, skála, mértékegység, alapérték, állás).
- Opcionális kapcsolat: `plugin.parameterId ↔ hardware.controlId`. A kijelzett 0–10 állás a **felhasználó által megadott fizikai referencia**, nem feltétlenül egyezik a plugin normalizált értékével.
- Hardveres mérés előtt a UI mutassa: „Állítsd a fizikai Gain potmétert 6.5/10-re”, majd kérjen manuális megerősítést.
- Fizikai potik automatizálása kizárólag megfelelő, tényleges vezérlési interfész esetén; általában manuális.
- Presetek, mérési pontok, mérési sorozatok és vezérlők kombinációi menthetők.

### 3.3 Mérési sorozat

1. Input/output device és loopback kalibráció.
2. Biztonságos jelszintellenőrzés, clipping detection, csatorna- és polarity ellenőrzés.
3. Tesztjel kiválasztása vagy automatikus terv alapján futtatás.
4. Azonos bemenő jel A és B feldolgozásán keresztül (hardvernél valósidejű capture, plugin esetén lehet offline rendering).
5. Latency alignment és **csak kontrollált, dokumentált** gain normalizálás; a szintkülönbséget önálló metrikaként is őrizni kell.
6. Analízis, riport, nyers fájlok mentése.
7. AI analysis → mérési hipotézis és következő vizsgálat / parameter optimization / source-aware change proposal.

### 3.4 AI-guided next measurement

A rendszer kérheti a következő célzott állást (pl. Presence=0/5/10, Resonance=5, Gain=3 és =8). **Nem vezérelhet automatikusan analóg fizikai potit.** Minden kérést a felhasználó jóváhagy, UI mutatja az állásokat, majd lehet Startot nyomni. A planner figyelje a mérési költséget és a paraméterek azonosíthatóságát. A terv mérnökileg indokolt, de bizonytalanságát jelölje.

## 4. Mérés — kötelező képességek

### 4.1 Tesztjelek

- Logaritmikus/exponenciális sweep, megfelelő dekonvolúcióval.
- Stepped sine több bemeneti szinten.
- Multitone intermodulációs teszt.
- Impulzus/burst, step/level sweep, zaj.
- Felhasználói DI gitár/basszus WAV tesztek és referencia wav.
- Változtatható peak/RMS szint, időtartam, csend előtte/utána, ismétlés.

### 4.2 Analízis

- Frekvenciaátvitel és frekvenciánkénti dB eltérés (nemlineáris rendszernél szintfüggő értelmezéssel).
- Harmonikus spektrum és THD vs. frekvencia/bemenő szint.
- IMD, dinamikus transfer, kompresszió, attack/recovery, transient/burst.
- Fázis/group delay csak megfelelő feltételezések és mérési feltételek mellett.
- A/B waveform és spektrális összehasonlítás, időbeli igazítás, null/residual elemzés — **nemlineáris rendszerekben a null-teszt nem önmagában minőségi mutató**.
- Zajpadló, hum, zajprofil.
- Determinisztikus loss/score, külön független validációs eseteken.
- Mérési bizonytalanság, megismételhetőség, capture clipping és drift figyelése.

### 4.3 Audio- és elektromos biztonság

- Csöves erősítő **speaker output soha nem köthető közvetlenül hangkártya line inputra**.
- Speaker output méréséhez megfelelő impedanciájú és teljesítményű, jóváhagyott load box/terhelés és biztonságos line out szükséges.
- Az FX Send csak az előfokot méri; teljes erősítőnél az alkalmazott terhelés, load box és/vagy kabinet befolyásolja a végfok reakcióját.
- Reamp/DI jelszintek, impedancia, ground loop, interface gain, polarity, sample rate és clocking dokumentálandó.
- Mérési start előtt routing-ellenőrzés és maximum output limiter/gain guard; a szoftver nem helyettesíti a megfelelő hardveres elektromos védelmet.
- Magas zaj-/feedback-szintnél megszakítható mérés és vészleállítás.

## 5. Összehasonlítási célok — AudioTarget szerződés

`AudioTarget` logikai interfész (nem feltétlen egyetlen szinkron `process()` API):

- `PluginTarget` (VST3, AU macOS-en), saját process és state management.
- `PluginChainTarget` (soros útvonal MVP-ben; párhuzamos később).
- `HardwareTarget` (asynchronous playback/capture, kalibrált round-trip latency).
- `FileTarget` (reprodukálható előre rögzített output; input stimulus és mérés metaadata kötelező).

A hardware nem biztosít sample-szinkron, azonnali `process()` hívást. Ezért a renderelési réteg két módot adjon: `renderOffline()` és `measureRealtime()`; közös normalizált eredmény: `MeasurementArtifact`.

## 6. AI/agent-integráció — kezdetben Codex vagy Claude

### 6.1 Adapteres megközelítés

Az AI-réteg cserélhető; semmilyen modellhez ne legyen hozzákötve az audio core.

```text
AiAdvisor
├── NoAiAdvisor
├── CodexAdvisor
├── ClaudeAdvisor
└── LocalLlmAdvisor (future)
```

Az első kiadásban **ne ígérjük** a Codex/Claude előfizetés automatikus újrafelhasználását. Vizsgáljuk meg a hivatalosan támogatott integrációt (API, CLI/agent SDK, bejelentkezési mód, szolgáltatási feltételek), majd készítsünk támogatott adaptereket. A helyileg telepített CLI hívása is csak dokumentált, felhasználó által jóváhagyott munkafolyamatban történhet. Az API kulcsokat OS keychainben tároljuk, ne session JSON-ban és ne logokban.

### 6.2 Az AI teendői

- Méretezett, kompakt, strukturált measurement summary értelmezése.
- Több jelölt hibahipotézis és confidence/evidence megadása.
- Következő, információt növelő mérés javaslata.
- Paraméterillesztési feladat összeállítása a numerikus optimizer számára.
- A projektforrás vagy `dsp-lab.json` alapján célzott kódmódosítási *javaslat*.
- Opcionális PR/patch generálás felhasználó jóváhagyása után, verziókezelt workspace-ben.
- Mérési eredmények és DSP-kód alapján reprodukálható fejlesztési riport.

### 6.3 Tiltott / korlátozott műveletek

- AI közvetlenül ne processzáljon a real-time audio threaden.
- AI ne állítson automatikusan fizikai erősítő jelszintet, potmétert vagy routingot.
- AI által generált shell/build parancs csak allowlist és megerősítés mellett futtatható.
- Forráskód-módosítás kizárólag külön branch/worktree vagy diff/patch formában, explicit jóváhagyással.
- Megjelenő diagnózis ne állítsa biztosra a konkrét áramköri hibát pusztán black-box mérésből.
- Fájlok, kapcsolási rajzok, source és audio felhőbe küldése külön, átlátható opt-in.

### 6.4 Strukturált tool/action API

Az LLM csak ellenőrzött parancsokat javasolhat:

```json
{
  "action": "request_measurement",
  "target": "reference",
  "controls": {"presence": 10, "resonance": 5},
  "stimuli": [{"type": "stepped_sine", "levelsDbFS": [-30, -20, -10]}],
  "reason": "Disambiguate level-dependent high-frequency roll-off",
  "confidence": 0.67
}
```

Más megengedett actionök: `analyze_results`, `optimize_parameters`, `propose_code_patch`, `request_circuit_data`, `no_change`.

A JSON Schema-t validáljuk. A `confidence` becslés, nem objektív valószínűség. Minden művelethez felhasználói jóváhagyás és mérési/projekt-scope szükséges.

## 7. Numerikus optimalizáció — nem LLM számol

- Determinisztikus és black-box optimalizáló, változtatható paraméterhatárokkal.
- Kezdetben derivative-free (pl. CMA-ES vagy differential evolution) és egyszerű lokális keresés.
- Később differentiable modellekhez gradient-based illesztés, Bayesian optimization drága hardware mérésekhez.
- Objective lehet súlyozott frekvencia-, harmonikus-, dinamikai-, IMD- és tranzienshiba.
- Paraméterek min/max, logaritmikus skála, discrete values, függőségek és freeze státusz.
- Candidate rendszer több ponton validálva: gain level, frekvencia, potméterállások és korábban nem látott DI jelek.
- Ne kozmetikázza a kimenetet utólag EQ-val, ha a cél a valós DSP-topológia pontosítása, hacsak a felhasználó ezt kifejezetten nem engedélyezi.
- A hibaszámot és a bizonytalanságot nyersen tároljuk; egyetlen összesített similarity score ne fedje el az eltérések természetét.

## 8. Opcionális circuit-aware mód

- Beolvasás: kapcsolási rajz PDF/kép és lehetőleg SPICE netlist.
- Kép/PDF esetén az AI adjon *javasolt* netlistet/topológiai gráfot, de felhasználói ellenőrzés szükséges.
- Azonosítható fokozatok: gain stage, coupling filters, tone stack, global negative feedback, presence/resonance, power amp/sag.
- Az AI kérhet konkrét potméterállásokat az azonosíthatóság növelésére.
- A feldolgozás circuit-aware része fokozatosan épüljön: v1 manuális circuit annotations; később netlist parser, majd WDF/state-space alkatrészmodellek.
- Nem vállalható biztos fizikai topológia-rekonstrukció kizárólag bemenet–kimenet adatokból.

## 9. Forráskód-integráció meglévő pluginhez

A felhasználó kiválaszthatja a source project rootot. A plugint külön host process futtassa crash isolationnel. A source-aware workflow opcionális `dsp-lab.json` manifestet használjon:

```json
{
  "schemaVersion": 1,
  "pluginId": "moth.amp",
  "projectRoot": ".",
  "build": {
    "workingDirectory": ".",
    "command": "cmake --build build --config Release"
  },
  "parameters": [
    {
      "id": "preamp.stage1.cutoffHz",
      "min": 500,
      "max": 20000,
      "scale": "logarithmic",
      "default": 8000,
      "optimizable": true
    }
  ]
}
```

A manifest önmagában **nem garantál** futásidejű hozzáférést a belső változókhoz: ehhez explicit developer bridge, build-time config generálás vagy source patch + rebuild kell. Így a hostolható plugint és a source modelt külön interfészek kezelik.

## 10. Javasolt technikai stack

- **Core:** C++20, JUCE (audio device, DSP és plugin hosting); CMake.
- **UI:** JUCE natív UI első körben; React/WebView csak akkor, ha a komplexitás és fenntartási költség indokolja.
- **FFT:** JUCE DSP vagy megfelelő natív FFT library.
- **Tárolás:** SQLite metadata + lokális WAV/FLAC és JSON artifacts.
- **Optimization:** C++ optimization library vagy izolált Python worker kezdetben; a worker nem dolgozhat real-time audio threadben.
- **AI bridge:** aszinkron, cserélhető `AiAdvisor`, Codex/Claude adapterek, később llama.cpp/Ollama.
- **OS:** macOS elsőként (Apple Silicon), Windows 11 második, Linux harmadik.
- **Teszt:** GoogleTest/Catch2, golden audio fixtures, CI headless offline rendering, mérésismételhetőségi tesztek.

## 11. Architektúra

```text
Desktop UI
  ├── Project / Session Management
  ├── Routing & Hardware Safety
  ├── Plugin Browser & Parameter Mapping
  ├── Measurement Plan & Guided Hardware Controls
  ├── Graphs / A-B / Residual / Report
  └── AI Suggestions & Approval
                │
                ▼
Application Services
  ├── AudioTarget Registry
  ├── Session Orchestrator
  ├── Measurement Planner
  ├── Artifact Manager
  ├── Analysis Engine
  ├── Optimization Engine
  ├── Regression Engine
  └── AiAdvisor Gateway
                │
       ┌────────┼──────────────┐
       ▼        ▼              ▼
  Audio Engine  Storage    External AI (opt-in)
    ├─ Host       ├─SQLite    ├─Codex
    ├─ I/O        ├─WAV       └─Claude
    ├─ Stims      └─JSON
    ├─ Capture
    ├─ Calibrate
    └─ Align
```

Külön folyamatban vagy megfelelő crash boundary mellett fusson a harmadik féltől származó plugin. A teljes méréshez audit log és reprodukálhatósági metaadat tartozzon.

## 12. Példa domain adatmodell

```text
Project
  ├── AudioTargetReference
  ├── AudioTargetCandidate
  ├── HardwareControlMappings
  ├── MeasurementProfiles
  ├── MeasurementSessions
  │     ├── StimulusSpec
  │     ├── RoutingCalibration
  │     ├── ControlSnapshot
  │     ├── ReferenceCapture
  │     ├── CandidateCapture
  │     ├── AnalysisResult
  │     └── EnvironmentMetadata
  ├── OptimizationRuns
  ├── AiRecommendations
  └── RegressionBaselines
```

Minden artefakt tartalmazzon verziót, UTC timestampet, sample rate-et, buffer size-ot, gain/routing állapotot, plugin azonosítót/verziót, tesztjel hash-t, feldolgozás verzióját és ahol értelmes, source Git commit SHA-t.

## 13. Eredmény export

```text
moth-dsp-lab-project/
├── project.json
├── sessions/
│   └── 2026-11-02T170000Z/
│       ├── manifest.json
│       ├── stimulus.wav
│       ├── reference.wav
│       ├── candidate.wav
│       ├── frequency_response.json
│       ├── harmonic_analysis.json
│       ├── dynamics.json
│       ├── residual.wav
│       ├── analysis_summary.json
│       ├── recommendations.json
│       └── report.md
├── optimization/
└── regression/
```

Opcionális `codex-context.md` vagy `claude-context.md` export: tömör summary, módszertan, paraméterek, elvárt mérnöki eredmény, elérhető artifact pathok. Ne töltsük fel automatikusan a teljes nyers hangot vagy forráskódot.

## 14. Szállítási roadmap

### M0 — Repository skeleton

- CMake/JUCE desktop app; moduláris `core`, `audio-host`, `measurement`, `analysis`, `optimization`, `ai-bridge`, `ui`.
- Egységtesztek, egyszerű session storage, plugin scan UI.

### M1 — Offline Plugin A/B MVP

- Két VST3 vagy pluginlánc betöltése.
- Azonos WAV/DI input offline feldolgozása.
- Waveform/spectrum, szint, latency alignment, residual, egyszerű dB differencia.
- Plugin state és mérés mentése/visszatöltése.
- **Acceptance:** két különböző plugin kimenete reproducibilisen összehasonlítható, AI nélkül.

### M2 — Fizikai hardver mérés

- Hangkártya csatorna routing, audio playback/capture, loopback kalibráció, gain guards.
- Hardverpotik felülete, fizikai állások manual confirm, precíz beállítás-snapshot.
- Stepped sine/sweep, THD, alap dinamikai metrikák.
- **Acceptance:** megfelelő külső biztonságos jelláncon át referencia hardver mérhető, összehasonlítható offline plugin jelével.

### M3 — Codex/Claude advisor

- Engedélyezett adapter-integráció, provider config, credential storage.
- Strukturált AI summary, hypothesis, request next measurement, optimization plan.
- Opt-in adatküldés megtekinthető preview-val.
- **Acceptance:** internet nélkül is használható minden mérés; AI bekapcsolva a jóváhagyott mérési csomag alapján célzott tervet ad.

### M4 — Parameter matching és regresszió

- Host-publikus paraméterek optimizerrel variálhatók.
- Több jelszint és potméterállás, validációs tesztek, trend/score riport.
- **Acceptance:** ismert, szintetikus elhangolt pluginparaméter visszailleszthető, és az eredményt független teszt igazolja.

### M5 — Source-aware DSP refinement

- Projektfolder + manifest + manuális developer bridge / build-config adapter.
- Patch proposal, diff, build/test, re-render, regresszió.
- **Acceptance:** elkülönített módosítás esetén visszaállítható és mérhető az előtte/utána különbség.

### M6 — Circuit-aware measurement planner

- Schematic/netlist csatolás, kézi megerősített circuit metadata.
- Paraméter-azonosíthatóság és célzott hardware control állások.
- **Acceptance:** kontrollállások és tesztsignálok reprodukálható mérési tervvé alakulnak, az eredményekből ellenőrizhető hipotézisek születnek.

### Later — Offline local LLM

- `LocalLlmAdvisor` a már kész `AiAdvisor` API-n; cserélhető GGUF/llama.cpp vagy Ollama backend.
- Opcionális finomhangolás saját, ellenőrzött szintetikus és valós mérési adatokból.
- A mérőmotor AI nélkül változatlanul működik.

## 15. MVP-ből szándékosan kimarad

- Neural capture vagy neurális amp-modell tanítása.
- Automatikus elektronikai topológia felismerésének garantálása képből.
- Teljes automatikus forráskód átírás jóváhagyás nélkül.
- Fizikai kezelőszervek motoros mozgatása.
- Online account/backend, felhős kötelező szinkron.
- Fizikai hardware zárt hurkú optimalizálása nagy számú mérésből felhasználói részvétel nélkül.
- Az összes pluginformátum támogatásának ígérete első verzióban.

## 16. Fejlesztő agent utasítása

**Codex / Claude Code, ennek a dokumentumnak az alapján:**

1. Először készíts rövid, végrehajtható implementációs tervet a M0 és M1 mérföldkőhöz; válaszd szét az audio real-time és az offline analysis code pathokat.
2. Hozz létre tiszta, buildelhető CMake/JUCE project skeleton-t, külön interfészekkel (`AudioTarget`, `MeasurementEngine`, `AnalysisEngine`, `SessionStore`, `AiAdvisor`).
3. Implementálj két VST3 plugin hostolását és azonos WAV input determinisztikus offline renderelését; támogasd a sample rate és block size beállítást, plugin state mentést, tail handlinget és alap latency kompenzációt.
4. Adj FFT-spektrum, gain-eltérés, időbeli alignment és residual összehasonlító nézetet, mérési artifact exporttal.
5. Írj automatikus teszteket szintetikus DSP fixture-ökkel (gain, EQ, clipping, delay), amelyeknél a várt eltérés ismert.
6. Ne építs még automatikus kódjavítót vagy neurális modellt; az AI adapternek készíts világos interfészt és `NoAiAdvisor` stubot.
7. Dokumentáld a build/run/test parancsokat és a következő mérföldkőre javasolt feladatokat.
8. Ha meglévő repositoryban dolgozol, előbb auditáld a build rendszert és a már használatos DSP/audio könyvtárakat; csak indokolt esetben cserélj technológiát.

### Definition of Done (M1)

- GUI-ban két VST3 plugin kiválasztható és külön kezelhető.
- Ugyanaz a WAV bemenet szólaltatható meg mindkét pluginon, reprodukálható outputtal.
- Van megjeleníthető A/B waveform és spektrum; tárolható a különbség és a mérési session.
- Kezelhető eltérő plugin latency és render tail, vannak basic channel mapping opciók.
- A/B elemzés AI kapcsolat nélkül működik.
- Ismeretlen vagy hibás plugintól az alkalmazás nem veszít el mentett mérési adatokat.
- A tesztek átmennek a fejlesztői gépen, dokumentált a build és indítás.

---

**Design principle:** a tudományos mérés igazságforrása a determinisztikus audio/analízis motor. A Codex/Claude advisor következtetéseket és fejlesztési tervet ad, de a fizikai mérés, optimalizáció és regressziós validáció eredménye dönti el, hogy egy javasolt változtatás valóban előrelépés-e.
