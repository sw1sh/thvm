// thvm-atp: WL-independent CLI for thvm's ATP completion engine.
//
// Exposes the same knob surface as TFindProof's Method option, mapped
// onto the engine's env-var toggles, and runs the exact code path of
// tests/test_atp_wolfram_bench.c (this file includes that translation
// unit with its main() renamed, so a CLI run is byte-identical to a
// bench run given the same knobs).
//
//   thvm-atp [--preset=waldmeister|waldmeister-lazy|none]
//            [--<knob>[=<value>] ...]
//            [--max-steps=N] [--wall=SEC] [--szs]
//            problem.pr
//
// A multi-goal CONCLUSION block is proved in one run (all goals must
// join), matching `wmcli -auto` / FindEquationalProof semantics.
// --list-knobs prints the full knob table with the TFindProof Method
// suboption each one mirrors.

#define main atp_bench_main
#include "../tests/test_atp_wolfram_bench.c"
#undef main

#include <getopt.h>

// One CLI knob: kebab-case flag (named after the TFindProof Method
// suboption), the engine env var it drives, whether it takes a value
// (0 = boolean 0/1 with default 1 when bare), and the Method name.
typedef struct {
  const char *flag;
  const char *env;
  int         has_value;
  const char *method;
} AtpKnob;

static const AtpKnob KNOBS[] = {
  {"cp-weight",              "THVM_ATP_CP_WEIGHT",       1, "CriticalPairWeight"},
  {"max-weight",             "THVM_ATP_MAXW",            1, "MaxWeight"},
  {"auto-max-weight",        "THVM_ATP_AUTO_MAXW",       1, "AutoMaxWeight"},
  {"max-weight-slope",       "THVM_ATP_MAXW_SLOPE",      1, "AutoMaxWeight slope"},
  {"selection-ratio",        "THVM_ATP_SELECTION_RATIO", 1, "SelectionRatio"},
  {"goal-interleave",        "THVM_ATP_GOAL_INTERLEAVE", 1, "GoalInterleave"},
  {"ground-join",            "THVM_ATP_GROUND_JOIN",     0, "GroundJoin"},
  {"connectedness",          "THVM_ATP_CONNECT",         0, "Connectedness"},
  {"rhs-interreduce",        "THVM_ATP_RHS_IR",          0, "RHSInterreduce"},
  {"unfailing-cp",           "THVM_ATP_UNFAILING",       0, "UnfailingCP"},
  {"cp-set-interreduce",     "THVM_ATP_CP_SET_IR",       0, "CPSetInterreduce"},
  {"cp-set-ir-period",       "THVM_ATP_CP_SET_IR_PERIOD",1, "CPSetInterreduce period"},
  {"lazy-normalize",         "THVM_ATP_LAZY_NORM",       0, "LazyNormalize"},
  {"demote-on-lhs-simplify", "THVM_ATP_WM_DEMOTE",       0, "DemoteOnLhsSimplify"},
  {"orphan-murder",          "THVM_ATP_ORPHAN",          0, "OrphanMurder"},
  {"pop-subsume",            "THVM_ATP_POP_SUBSUME",     0, "PopSubsume"},
  {"eset-subsume",           "THVM_ATP_ESET_SUBSUME",    0, "ESetSubsume"},
  {"queue-subsume",          "THVM_ATP_QUEUE_SUBSUME",   0, "QueueSubsume"},
  {"flat-subsume",           "THVM_ATP_FLAT_SUBSUME",    0, "FlatSubsume"},
  {"comm-subsume",           "THVM_ATP_COMM_SUBSUME",    0, "CommSubsume"},
  {"emission-order",         "THVM_ATP_EMISSION_ORDER",  0, "EmissionOrder"},
  {"intake-order",           "THVM_ATP_INTAKE_ORDER",    0, "IntakeOrder"},
  {"mixmost-nf",             "THVM_ATP_MIXMOST_NF",      0, "MixmostNF"},
  {"formation-fifo",         "THVM_ATP_FORMATION_FIFO",  0, "FormationFifo"},
  {"fifo-threshold",         "THVM_ATP_FIFO_THRESHOLD",  1, "FIFO interleave threshold"},
  {"eset-distdir",           "THVM_ATP_ESET_DISTDIR",    0, "EsetDistdir"},
  {"drain-chainpos",         "THVM_ATP_DRAIN_CHAINPOS",  0, "FormationFifo drain (chainpos)"},
  {"drain-revface",          "THVM_ATP_DRAIN_REVFACE",   0, "FormationFifo drain (revface)"},
  {"mnf",                    "THVM_ATP_MNF",             0, "GoalDirected (MNF front)"},
  {"record-norm",            "THVM_ATP_RECORD_NORM",     0, "RecordNorm"},
  {"free-var-instance",      "THVM_ATP_FVI",             0, "FreeVarInstance"},
  {"implicit-cp",            "THVM_ATP_IMPLICIT_CP",     0, "UseImplicitCp"},
  {"einsstern",              "THVM_ATP_EINSSTERN",       0, "Einsstern"},
  {"no-overlap-below-skolem","THVM_ATP_NO_OVERLAP_BELOW_SKOLEM", 0, "NoOverlapBelowSkolem"},
  {"reclassify",             "THVM_ATP_RECLASSIFY",      0, "Reclassify"},
  {"reversed-completion",    "THVM_ATP_REVERSED",        0, "ReversedCompletion"},
  {"sue-management",         "THVM_ATP_SOS",             0, "SUEManagement"},
  {"backward-ground-join",   "THVM_ATP_BWD_GROUND_JOIN", 0, "BackwardGroundJoin"},
  {"cp-side",                "THVM_ATP_CP_SIDE",         0, "CPSide"},
  {"wm-trie-faithful",       "THVM_ATP_WM_TRIE_FAITHFUL",0, "(faithful trie)"},
  {"initial-ultimate",       "THVM_ATP_INITIAL_ULTIMATE",0, "IntakeOrder initial stamp"},
  {"database-ultimate",      "THVM_ATP_DATABASE_ULTIMATE",0,"(WM database=ultimate)"},
  {"incremental-ir",         "THVM_ATP_INCR_IR",         0, "(incremental interreduce)"},
  {"cp-index",               "THVM_ATP_CP_INDEX",        0, "(CP index)"},
  {"right-reduce",           "THVM_ATP_RIGHT_REDUCE",    0, "(right reduce)"},
  {"lr-sortieren",           "THVM_ATP_LR_SORTIEREN",    0, "(WM LR sort)"},
  {"w2-mode",                "THVM_ATP_W2_MODE",         1, "(FIFO w2 mode)"},
  {"w2-modulo",              "THVM_ATP_W2_MODULO",       1, "(FIFO w2 modulo)"},
  {"perm-subsume",           "THVM_ATP_PERM_SUB",        0, "(permutative subsume)"},
  {"random-ratio",           "THVM_ATP_RANDOM_RATIO",    1, "(random CP ratio)"},
  {"random-seed",            "THVM_ATP_RANDOM_SEED",     1, "(random seed)"},
  {"profile",                "THVM_ATP_PROFILE",         1, "(profiling level)"},
  {"diag",                   "THVM_ATP_DIAG",            1, "(diagnostics)"},
};
static const int N_KNOBS = (int)(sizeof(KNOBS) / sizeof(KNOBS[0]));

static void usage(FILE *fp) {
  fprintf(fp,
    "usage: thvm-atp [options] problem.pr\n"
    "  --preset=waldmeister        TFindProof Method -> \"Waldmeister\" stack\n"
    "  --preset=waldmeister-lazy   Method -> \"WaldmeisterLazy\" stack\n"
    "  --preset=none               bare engine defaults\n"
    "  --<knob>[=v]                individual Method suboptions (override preset);\n"
    "                              bare boolean flags mean =1 (--list-knobs for all)\n"
    "  --max-steps=N               CP-processing cap        (default 500000)\n"
    "  --wall=SEC                  wall-clock cap           (default 120)\n"
    "  --szs                       print an SZS status line\n"
    "  --list-knobs                print the knob table and exit\n");
}

int main(int argc, char **argv) {
  const char *preset = "waldmeister";
  const char *file = NULL;
  const char *max_steps = "500000";
  const char *wall = "120";
  int szs = 0;

  for (int i = 1; i < argc; i++) {
    char *a = argv[i];
    if (strncmp(a, "--", 2) != 0) {
      if (file != NULL) { fprintf(stderr, "thvm-atp: one problem file only\n"); return 2; }
      file = a;
      continue;
    }
    char *name = a + 2;
    char *val = strchr(name, '=');
    size_t nlen = (val != NULL) ? (size_t)(val - name) : strlen(name);
    if (val != NULL) val++;
    if (strncmp(name, "help", nlen) == 0 && nlen == 4) { usage(stdout); return 0; }
    if (strncmp(name, "list-knobs", nlen) == 0 && nlen == 10) {
      for (int k = 0; k < N_KNOBS; k++)
        printf("  --%-26s %-34s %s\n", KNOBS[k].flag, KNOBS[k].env, KNOBS[k].method);
      return 0;
    }
    if (strncmp(name, "szs", nlen) == 0 && nlen == 3) { szs = 1; continue; }
    if (strncmp(name, "preset", nlen) == 0 && nlen == 6) { preset = (val != NULL) ? val : "waldmeister"; continue; }
    if (strncmp(name, "max-steps", nlen) == 0 && nlen == 9) { max_steps = val; continue; }
    if (strncmp(name, "wall", nlen) == 0 && nlen == 4) { wall = val; continue; }
    int hit = 0;
    for (int k = 0; k < N_KNOBS; k++) {
      if (strlen(KNOBS[k].flag) == nlen && strncmp(name, KNOBS[k].flag, nlen) == 0) {
        if (KNOBS[k].has_value && val == NULL) {
          fprintf(stderr, "thvm-atp: --%s needs a value\n", KNOBS[k].flag);
          return 2;
        }
        setenv(KNOBS[k].env, (val != NULL) ? val : "1", 1);
        hit = 1;
        break;
      }
    }
    if (!hit) { fprintf(stderr, "thvm-atp: unknown option --%.*s\n", (int)nlen, name); usage(stderr); return 2; }
  }
  if (file == NULL) { usage(stderr); return 2; }
  if (max_steps == NULL || wall == NULL) { usage(stderr); return 2; }

  // Presets replicate the WL $AtpPresetDefaults stacks.  Individual
  // knob flags above already setenv'd, and setenv(..., 0) here never
  // overwrites them, so flags override the preset exactly like Method
  // suboptions override a preset name.
  if (strcmp(preset, "waldmeister") == 0) {
    setenv("THVM_ATP_WALDMEISTER", "1", 0);
  } else if (strcmp(preset, "waldmeister-lazy") == 0) {
    setenv("THVM_ATP_WALDMEISTER", "1", 0);
    setenv("THVM_ATP_LAZY_NORM", "1", 0);
    setenv("THVM_ATP_CP_SET_IR", "1", 0);
    setenv("THVM_ATP_AUTO_MAXW", "30", 0);
  } else if (strcmp(preset, "none") == 0) {
    // bare engine defaults
  } else {
    fprintf(stderr, "thvm-atp: unknown preset %s\n", preset);
    return 2;
  }

  char *bench_argv[5];
  bench_argv[0] = (char *)"thvm-atp";
  bench_argv[1] = (char *)file;
  bench_argv[2] = (char *)max_steps;
  bench_argv[3] = (char *)wall;
  bench_argv[4] = NULL;
  int rc = atp_bench_main(4, bench_argv);

  if (szs) {
    const char *tag =
        (g_atp_bench_status == (int)ATP_PROVED)      ? "Theorem" :
        (g_atp_bench_status == (int)ATP_REFUTED)     ? "CounterSatisfiable" :
        (g_atp_bench_status == (int)ATP_QUEUE_EMPTY) ? "GaveUp" :
                                                       "ResourceOut";
    printf("%% SZS status %s for %s\n", tag, file);
  }
  if (g_atp_bench_status == (int)ATP_PROVED) return 0;
  return (rc != 0) ? rc : 1;
}
