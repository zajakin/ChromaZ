#include "cli.h"
#include <QCoreApplication>

namespace Cli {

void setupParser(QCommandLineParser &p) {
  p.setApplicationDescription(
      "ChromaZ — contig alignment viewer and Sanger chromatogram editor.\n"
      "Files and options given on the command line are applied right after start-up.");
  p.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
  p.addHelpOption();
  p.addVersionOption();

  p.addPositionalArgument("files",
      "Sequencing files to open: .ab1 .scf .fasta .fa .gb .gbk .fastq .fq .txt, "
      "or a .ChromaZ project (a project replaces the current workspace).", "[files...]");

  p.addOptions({
    {{"p", "project"}, "Open a .ChromaZ project.", "file"},
    {{"r", "reference"}, "Use the first record of a FASTA/GenBank file as the reference (default: longest read).", "file"},
    {{"a", "algorithm"},
        "Alignment algorithm: cap3 (default), gotoh, nw, sw, blast, banded, wfa, clustal (progressive MSA), or 0-7.", "name"},
    {{"i", "min-identity"}, "Minimum identity (%) for a read to join the contig (default 45).", "percent"},
    {{"t", "table"}, "Genetic code: standard (default), mito, bacterial, or 0-2.", "name"},
    {"no-auto-rc", "Disable automatic reverse-complement detection."},
    {"iupac", "Use IUPAC ambiguity codes in the consensus."},
    {"refine", "Consensus refinement rounds: re-align reads to the built consensus (0-10, default 0).", "n"},
    {"threads", "Alignment worker threads (0 = automatic).", "n"},
    {"no-orf-a", "Hide the 6-frame translation of the reference (ORF A)."},
    {"no-orf-b", "Hide the 6-frame translation of the consensus (ORF B)."},
    {"restriction", "Show restriction sites."},
    {"trim-quality", "Trim low-quality read ends below this Phred score after alignment.", "phred"},
    {"trim-vectors", "Trim cloning-vector / primer sequences from read ends after alignment."},
    {"export-fasta", "Write the consensus as FASTA.", "file"},
    {"export-gbk", "Write the consensus as annotated GenBank.", "file"},
    {"export-png", "Save the alignment view as PNG.", "file"},
    {"export-pdf", "Write a PDF alignment report.", "file"},
    {"pdf-bases", "Bases per line in the PDF report (default 60).", "n"},
    {"save-project", "Save the resulting workspace as a .ChromaZ project.", "file"},
    {"quit", "Exit after performing the requested exports (batch mode)."},
  });
}

QString helpText() {
  QCommandLineParser p;
  setupParser(p);
  QString t = p.helpText();
  t += "\nExamples:\n"
       "  ChromaZ read1.ab1 read2.ab1 --algorithm clustal --refine 2\n"
       "  ChromaZ -p sample.ChromaZ --export-fasta consensus.fasta --quit\n"
       "  ChromaZ reads/*.ab1 -r plasmid.gb --trim-quality 20 --trim-vectors --export-pdf report.pdf --quit\n"
       "\nIn batch mode (--quit) run with QT_QPA_PLATFORM=offscreen on a machine without a display.\n";
  return t;
}

int parseAlgorithm(const QString &text) {
  const QString t = text.trimmed().toLower();
  static const char *names[] = {"cap3", "gotoh", "nw", "sw", "blast", "banded", "wfa", "clustal"};
  for (int i = 0; i < 8; ++i) if (t == names[i]) return i;
  bool ok = false;
  int v = t.toInt(&ok);
  return (ok && v >= 0 && v <= 7) ? v : -1;
}

int parseTable(const QString &text) {
  const QString t = text.trimmed().toLower();
  if (t == "standard") return 0;
  if (t == "mito" || t == "mitochondrial") return 1;
  if (t == "bacterial" || t == "plastid") return 2;
  bool ok = false;
  int v = t.toInt(&ok);
  return (ok && v >= 0 && v <= 2) ? v : -1;
}

} // namespace Cli
