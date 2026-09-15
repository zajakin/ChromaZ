# ChromaZ — Contig Alignment Viewer & Chromatogram Editor

![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)
![Qt6](https://img.shields.io/badge/Framework-Qt%206-green.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-orange.svg)

**Languages: [English](README.md) | [Русский](README_RU.md)**

**ChromaZ** is a cross-platform desktop C++/Qt6 application for contig assembly, Sanger sequencing chromatogram visualization, multiple sequence alignment, and bioinformatics analysis of nucleotide and amino acid sequences.

---

## 📥 Downloads & Quick Installation

Pre-compiled standalone installers are available directly in the root of this repository:

* **Debian / Ubuntu**: [Download ChromaZ_Linux_Install_VERSION_amd64.deb](https://github.com/zajakin/ChromaZ/releases/latest/).
* **Windows**: [Download ChromaZ_Windows_Install_VERSION.exe](https://github.com/zajakin/ChromaZ/releases/latest/).
* **macOS**: [Download ChromaZ_macOS_Install_VERSION.dmg](https://github.com/zajakin/ChromaZ/releases/latest/)

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
  * **Internal Progressive Clustal** — Built-in progressive multiple sequence alignment (MSA) without external CLI dependencies.
* **Auto-Contig & Reference Support**: Automatic seed selection based on the most representative read, or custom reference sequence loading (FASTA/GenBank).
* **Reverse Complement Detection (Auto RC)**: Automatic strand orientation and alignment for complementary reads (`[RC]`) with manual toggle option.

---

### 📈 Chromatogram Analysis & Phred Quality Scores
* **Four-Color Chromatogram Plot**: Trace visualization for Sanger sequencing files (`.ab1` and `.scf`) using standard base colors (A — green, C — blue, G — black/yellow, T — red).
* **Phred Quality Scores**: Color-coded quality visualization per base (Green $Q \ge 30$, Yellow $Q \ge 20$, Red $Q < 20$).
* **Quality Trimming (Trim Low QC)**: Automated low-quality end-trimming based on Phred thresholds.
* **Nucleotide Coordinates & Ruler**: 1-based nucleotide position numbering displayed above each read, consensus, and reference (excluding gap columns).

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
* **SNP Navigation**: Single Nucleotide Polymorphism detection with quick jumping controls (`◀ SNP` / `SNP ▶`).
* **Fast Sequence Search**: Substring match highlighting with automatic viewport auto-scrolling.
* **Drag & Drop**: Track reordering via drag-and-drop and direct file loading onto the viewer.

---

### 📄 Export & Project Management
* **Copy Selection as Image**: Copy selected chromatogram segments along with trace peaks directly to the clipboard as PNG graphics.
* **FASTA Export**: Export generated consensus sequence.
* **PDF Export**: Multi-page PDF report generation with configurable bases per line.
* **PNG Export**: Save current alignment view as high-resolution raster images.
* **`.ChromaZ` Projects**: Save and load complete workspace states (strand orientations, gaps, edits, and undo history) in JSON format.

---

## 📁 Supported File Formats

| Format | Extension | Description |
| :--- | :--- | :--- |
| **ABI Chromatogram** | `.ab1` | Read sequence + trace peaks + Phred QC |
| **Staden SCF (v2/v3)** | `.scf` | Read sequence + trace peaks + Phred QC |
| **FASTA / GenBank** | `.fasta`, `.fa`, `.gb`, `.gbk` | Reference sequences or multi-record reads |
| **FASTQ** | `.fastq`, `.fq` | Sequence reads + Phred QC |
| **Plain Text** | `.txt` | Raw nucleotide lines |
| **ChromaZ Project** | `.ChromaZ` | Full assembly project workspace |

---

## 🎹 Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **Ctrl + O** | Open `.ChromaZ` project |
| **Ctrl + S** | Save `.ChromaZ` project |
| **Ctrl + Z** | Undo last action |
| **Ctrl + Y** / **Ctrl + Shift + Z** | Redo action |
| **Ctrl + C** | Copy selected sequence (DNA or ORF) |
| **Ctrl + F** | Find sequence |
| **Ctrl + Mouse Wheel** | Zoom Y-axis (Trace peak height) |
| **Mouse Wheel** | Zoom X-axis (Base horizontal spacing) |

---

## 🛠 Building & Compilation

### Prerequisites
* C++17 compliant compiler (GCC 9+, Clang 10+, MSVC 2019+)
* CMake 3.16+
* Qt 6.x (`Core`, `Gui`, `Widgets`, `Test` modules)

### Linux Build (Debian / Ubuntu / Fedora)
```bash
git clone [https://github.com/your-username/ChromaZ.git](https://github.com/your-username/ChromaZ.git)
cd ChromaZ
mkdir build && cd build
cmake src/.. && make -j$(nproc)
./ChromaZ
```

---

## 📜 License
This project is licensed under the MIT License. See the LICENSE file for details.

