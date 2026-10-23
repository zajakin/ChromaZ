# ChromaZ — Contig Alignment Viewer & Chromatogram Editor

![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)
![Qt6](https://img.shields.io/badge/Framework-Qt%206-green.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-orange.svg)

**Languages: [English](README.md) | [Русский](README_RU.md)**

**ChromaZ** is a cross-platform desktop C++/Qt6 application for contig assembly, Sanger sequencing chromatogram visualization, multiple sequence alignment, restriction mapping, and bioinformatics analysis of nucleotide and amino acid sequences.

---

## Screenshot

![Интерфейс ChromaZ](ChromaZ.png)

---

## 📥 Downloads & Quick Installation

Pre-compiled standalone installers are available on the [Releases](https://github.com/zajakin/ChromaZ/releases/latest/) page:

* **Debian / Ubuntu**: [Download ChromaZ_Linux_Install_VERSION_amd64.deb](https://github.com/zajakin/ChromaZ/releases/latest/)
* **Windows**: [Download ChromaZ_Windows_Install_VERSION.exe](https://github.com/zajakin/ChromaZ/releases/latest/)
* **macOS**: [Download ChromaZ_macOS_Install_VERSION.dmg](https://github.com/zajakin/ChromaZ/releases/latest/) (Universal binary for Apple Silicon & Intel)
* **Windows (Chocolatey)**: `choco install chromaz`

---

## 🌟 Key Features

### 🧬 Contig Assembly & Alignment Algorithms
* **7+ Independent Alignment Algorithms**:
  * **OLC / CAP3 (Sanger Default)** — Vector NTI ContigExpress style overlap assembly with zero penalty for 5'/3' overhangs.
  * **Affine-Gap Block (Gotoh)** — Semi-global alignment with separate gap-open and gap-extension penalties.
  * **Needleman-Wunsch** — Strict global alignment.
  * **Smith-Waterman** — Local alignment targeting regions of maximum identity.
  * **BLAST (Seed & Extend)** — Accelerated $k$-mer search with extension along the optimal diagonal.
  * **Banded DP** — Banded dynamic programming for long reads.
  * **Wavefront Alignment (WFA)** — Stepwise wavefront error-expansion algorithm.
  * **Clustal-style progressive MSA** — k-mer distance matrix → UPGMA guide tree → profile-to-profile alignment of all reads together with the reference (no external tools).
* **Auto-Contig & Reference Support**: Automatic seed selection (the longest read), or a custom reference sequence loaded from FASTA / GenBank / TXT (*File → Load Reference Sequence…*, toolbar button 🎯).
* **Background, multi-threaded alignment**: reads are aligned in parallel on all CPU cores in a worker thread; the UI stays responsive, with a progress bar and a *Cancel* button.
* **Consensus refinement**: optional rounds (*Refine* in the toolbar) re-align all reads to the consensus built in the previous round.
* **Reverse Complement Detection (Auto RC)**: Automatic strand orientation and alignment for complementary reads (`[RC]`) with manual toggle option.

---

### 📈 Chromatogram Analysis & Phred Quality Scores
* **Coverage Mini-map**: a dockable panel with read depth along the contig, a Phred-quality strip (green/yellow/red), SNP ticks and the visible region; click or drag to navigate. Statistics: covered columns, mean/max depth, low-quality columns.
* **Four-Color Chromatogram Plot**: Trace visualization for Sanger sequencing files (`.ab1` and `.scf`) using standard base colors (A — green, C — blue, G — black/yellow, T — red).
* **Phred Quality Scores**: Color-coded quality visualization per base (Green $Q \ge 30$, Yellow $Q \ge 20$, Red $Q < 20$).
* **Quality Trimming (Trim Low QC)**: Automated low-quality end-trimming based on Phred thresholds.
* **Nucleotide Coordinates & Ruler**: 1-based nucleotide position numbering displayed above each read, consensus, and reference (excluding gap columns).

---

### ✂️ Restriction Analysis & Vector Trimming
* **REBASE Database Integration**: Built-in support for the commercial REBASE restriction enzyme database (`bionetc.txt` with ~800 enzymes) with automatic build-time retrieval and fallback to Qt resources.
* **Enzyme Selection**: Choose which enzymes are displayed (filterable checklist, choice is remembered between sessions); sites of several enzymes at one position are merged into a single label.
* **Virtual Gel**: *Analysis → Virtual Gel…* simulates an agarose gel of the consensus digested with the chosen enzymes (linear or circular, optional undigested lane, DNA ladder, fragment table, PNG export). Enzymes ticked in *Select Enzymes to Display* are preselected.
* **Cleavage Site Visualization**: Real-time rendering of restriction cut positions with enzyme labels angled above the consensus track, supporting degenerate IUPAC recognition sites and Type IIS shifted cutters (e.g., BsaI).
* **UniVec Vector Trimming**: 1-click trimming of cloning vector backbones, sequencing primers, and adapters (M13, T7, SP6, etc.) from read ends using fuzzy pattern matching.

---

### 🔊 Audio Dictation (Text-to-Speech)
* **Voice Readout for Sequence Proofreading**: Built-in audio playback using native system speech synthesizers (`Qt6::TextToSpeech`).
* **Adjustable Reading Speed**: Context-menu speed control (0.5x, 1.0x, 1.5x) with comma-separated single-base pronunciation, allowing hands-free cross-verification against paper notes or external displays.

---

### 🔬 Molecular Biology & PCR Utilities
* **PCR Primer Melting Temperature ($T_m$)**: The *Properties* window shows GC%, the Wallace-rule and GC-formula $T_m$ immediately; after entering Na⁺/Mg²⁺ and primer concentrations it adds the nearest-neighbor $T_m$ (SantaLucia 1998) with ΔH, ΔS and ΔG.
* **1-Click NCBI BLASTn**: Send selected sequences or consensus directly to NCBI BLAST in your default web browser with query parameters pre-filled.
* **Physicochemical Properties & Protein Graphs**:
  * **GC Profile**: Interactive sliding-window curve of GC content across selected regions.
  * **Kyte-Doolittle Hydrophobicity**: Hydropathy plot for translated reading frames.
  * **Molecular Weight**: Total calculated peptide molecular weight (Da) with water loss compensation.

---

### 🧪 Translation & 6-Frame Open Reading Frames (ORF)
* **6 Open Reading Frames**: Translation and visualization of forward (+1, +2, +3) and reverse (-1, -2, -3) frames for protein-coding regions.
* **Independent `ORF A` and `ORF B` Toggles**:
  * **ORF A**: Toggle 6-frame translation display for the top Reference / Auto-Contig sequence.
  * **ORF B**: Toggle 6-frame translation display for the Consensus sequence.
* **Genetic Code Tables**: Support for Standard Code (Human), Vertebrate Mitochondrial, and Bacterial/Plant Plastid tables.
* **IUPAC Consensus**: Ambiguous base calculation (R, Y, S, W, K, M) for mismatched positions across reads.

---

### ✍️ Interactive Editing & Navigation
* **Keyboard Sequence Editing**: Direct base substitution (A, C, G, T, N, `-`), insertion, and deletion with automatic trace coordinate recalculation.
* **Full Undo / Redo**: Multi-step history stack (`Ctrl+Z` / `Ctrl+Y` or `Ctrl+Shift+Z`) up to 100 actions.
* **SNP Navigation**: Single Nucleotide Polymorphism detection with quick jumping controls (`◀ SNP` / `SNP ▶`); the active SNP column is highlighted across the header and consensus.
* **Interactive Guide Line**: a vertical crosshair follows the mouse over all tracks, so the same position can be compared in the reference, consensus and every read.
* **Fast Sequence Search**: Substring match highlighting with automatic viewport auto-scrolling; toolbar search box with *Find ◀ / ▶*.
* **Deleted Bases**: Deleting a base keeps it (and its trace/quality data) in the read: it is shown as a blank slot, can be restored by typing a letter, and survives re-alignment (the alignment inserts a gap column only if needed). Bases are removed for good only by end trimming.
* **Recent Files & Autosave**: *File → Open Recent*; the session is autosaved every 2 minutes and can be restored after a crash; unsaved changes are confirmed on exit.
* **Persistent Settings**: Window layout, algorithm, identity threshold, genetic code, ORF/restriction/IUPAC toggles and speech settings are restored on start.
* **Clear Workspace**: One-click reset of all loaded tracks.
* **Drag & Drop**: Track reordering via drag-and-drop and direct file loading onto the viewer.

---

### 📄 Export, Printing & Project Management
* **Direct Printing & Native Print Dialog**: Print multi-page alignment reports directly to physical or virtual printers via `Qt6::PrintSupport`.
* **Annotated GenBank (.gbk) Export**: Export consensus sequences with generated `LOCUS`, `FEATURES` (annotated restriction sites), and formatted `ORIGIN`.
* **Copy Selection as Image**: Copy selected chromatogram segments along with trace peaks directly to the clipboard as PNG graphics.
* **FASTA Export**: Export generated consensus sequence.
* **PDF Export**: Multi-page PDF report generation with configurable bases per line.
* **PNG Export**: Save current alignment view as high-resolution raster images.
* **`.ChromaZ` Projects**: Save and load workspace states (reads, traces, strand orientations, gaps and edits). Projects are stored as zlib-compressed JSON (typically several times smaller than plain JSON); projects saved by older versions as plain JSON still open. Saving is atomic, so a failure cannot damage an existing project file. Note: older ChromaZ versions cannot open the new compressed format.

---

## 📁 Supported File Formats

| Format | Extension | Description |
| :--- | :--- | :--- |
| **ABI Chromatogram** | `.ab1` | Read sequence + trace peaks + Phred QC |
| **Staden SCF (v2/v3)** | `.scf` | Read sequence + trace peaks + Phred QC |
| **FASTA / GenBank** | `.fasta`, `.fa`, `.gb`, `.gbk` | Reference sequences or multi-record reads |
| **FASTQ** | `.fastq`, `.fq` | Sequence reads + Phred QC |
| **Plain Text** | `.txt` | Raw nucleotide lines |
| **GenBank Export** | `.gbk` | Annotated consensus export with features |
| **ChromaZ Project** | `.ChromaZ` | Full assembly project workspace |

---

## 🎹 Keyboard Shortcuts & Actions

| Shortcut / Action | Description |
| :--- | :--- |
| **Ctrl + O** | Open sequencing files (AB1/SCF/FASTA/GenBank/FASTQ/TXT); projects — via *File → Open Project* |
| **Ctrl + S** | Save `.ChromaZ` project |
| **Ctrl + Z** | Undo last action |
| **Ctrl + Y** / **Ctrl + Shift + Z** | Redo action |
| **Ctrl + C** | Copy selected sequence (DNA or ORF) |
| **Ctrl + F** | Find sequence |
| **Ctrl + Mouse Wheel** | Zoom Y-axis (Trace peak height) |
| **Mouse Wheel** | Zoom X-axis (Base horizontal spacing) |
| **Right Click Menu** | BLASTn, properties & graphs (incl. $T_m$), TTS readout, GenBank export, enzyme selection |

## ⌨ Command-Line Options

```
ChromaZ [options] [files...]
```
Files and options are applied right after start-up (also available in the app: **Help → Command-line options…**).

| Option | Description |
| :--- | :--- |
| `files...` | Sequencing files (`.ab1 .scf .fasta .fa .gb .gbk .fastq .fq .txt`) and/or a `.ChromaZ` project |
| `-p, --project <file>` | Open a `.ChromaZ` project |
| `-r, --reference <file>` | Use the first record of a FASTA/GenBank file as the reference |
| `-a, --algorithm <name>` | `cap3` (default), `gotoh`, `nw`, `sw`, `blast`, `banded`, `wfa`, `clustal` (progressive MSA), or `0`–`7` |
| `-i, --min-identity <percent>` | Minimum identity for a read to join the contig (default 45) |
| `-t, --table <name>` | Genetic code: `standard`, `mito`, `bacterial` (or `0`–`2`) |
| `--no-auto-rc` | Disable automatic reverse-complement detection |
| `--iupac` | IUPAC ambiguity codes in the consensus |
| `--refine <n>` | Consensus refinement rounds (0–10, default 0) |
| `--threads <n>` | Alignment threads (0 = automatic) |
| `--no-orf-a`, `--no-orf-b` | Hide the 6-frame translation of the reference / consensus |
| `--restriction` | Show restriction sites |
| `--trim-quality <phred>` | Trim low-quality read ends after alignment |
| `--trim-vectors` | Trim vector / primer sequences from read ends |
| `--export-fasta`, `--export-gbk`, `--export-png`, `--export-pdf <file>` | Write the consensus / view / report |
| `--pdf-bases <n>` | Bases per line in the PDF report (default 60) |
| `--save-project <file>` | Save the resulting workspace |
| `--quit` | Exit after the requested steps (batch mode; use `QT_QPA_PLATFORM=offscreen` without a display) |
| `-h, --help`, `-v, --version` | Help / version |

Options given on the command line do not change the saved settings. Examples:
```bash
ChromaZ read1.ab1 read2.ab1 --algorithm clustal --refine 2
ChromaZ -p sample.ChromaZ --export-fasta consensus.fasta --quit
ChromaZ reads/*.ab1 -r plasmid.gb --trim-quality 20 --trim-vectors --export-pdf report.pdf --quit
```

---

## 🛠 Building & Compilation

### Prerequisites
* C++17 compliant compiler (GCC 11+, Clang 14+, MSVC 2019+)
* CMake 3.16+
* Qt 6.x (`Core`, `Gui`, `Widgets`, `Test`, `TextToSpeech`, `PrintSupport` modules)

### Linux Build (Debian Trixie / Ubuntu 24.04+)
```bash
git clone https://github.com/zajakin/ChromaZ.git
cd ChromaZ
cmake -B build -S src -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/ChromaZ
```

### Tests and signed releases (CI)
* Unit tests (`ctest`) run on Linux, Windows and macOS for every push and pull request. They cover SCF v2/v3 and GenBank parsing, alignment (including preservation of deleted bases), progressive MSA, consensus refinement, Tm and virtual-gel calculations.
* Releases are created once, after all three platforms build successfully, with a `SHA256SUMS.txt`.
* Installers are signed automatically when these repository secrets exist (otherwise unsigned installers are built):
  * **Windows** — `WIN_CERT_PFX_BASE64` (base64 of the `.pfx` code-signing certificate), `WIN_CERT_PASSWORD`
  * **macOS** — `MACOS_CERT_P12_BASE64` (Developer ID Application certificate, base64 `.p12`), `MACOS_CERT_PASSWORD`, `MACOS_SIGN_IDENTITY` (e.g. `Developer ID Application: Name (TEAMID)`)
  * **macOS notarization** — `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD` (app-specific password); the DMG is then notarized and stapled.

---

## 📜 License
This project is licensed under the MIT License. See the LICENSE file for details.
