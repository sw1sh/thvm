// profile.c - the THVM_ATP_PROFILE report, shared by every driver of the engine: the bench
// (tests/atp/test_atp_wolfram_bench.c), the FindProof CLI and THVMLink's run_proof (so
// bin/atp_replay and TFindProof print it too).  The counters are process totals: in a kernel
// that ran several proofs they cover all of them.

// THVM_ATP_PROFILE=1 arms the per-phase wall-clock counters inside thvm_atp_step (pop-normalize,
// CP generation, push-normalize, interreduce, goal check); =2 also the detail tier
// (push-normalize components, discrimination-tree and memo counters).  Gives 1 when armed.
fn u8 thvm_atp_profile_arm(void) {
  const char *pr = getenv("THVM_ATP_PROFILE");
  if (pr != NULL && (pr[0] == '1' || pr[0] == '2')) g_atp_phase_enabled = 1u;
  if (pr != NULL && pr[0] == '2') g_atp_phase_detail = 1u;
  return g_atp_phase_enabled;
}

// The report for run `s`, which took `el` seconds of wall time.
fn void thvm_atp_profile_print(AtpState *s, double el) {
  if (!g_atp_phase_enabled) {
    return;
  }
  double w = (el > 0.0) ? el : 1.0;
  u64 sumus = g_atp_phase_us_pop_normalize + g_atp_phase_us_cp_gen +
              g_atp_phase_us_push_normalize + g_atp_phase_us_interreduce +
              g_atp_phase_us_goal_check + g_atp_phase_us_cp_set_ir;
  double push_norm_us_per_cp = (s->n_cps_push_normalized > 0)
      ? (double)g_atp_phase_us_push_normalize /
        (double)s->n_cps_push_normalized
      : 0.0;
  printf("   phase: pop-norm=%.2fs (%.0f%%) cp-gen=%.2fs (%.0f%%)\n"
         "          push-norm=%.2fs (%.0f%%) [%.1fus/cp]"
         " interreduce=%.2fs (%.0f%%)\n"
         "          goal-check=%.2fs (%.0f%%) cp-set-ir=%.2fs (%.0f%%)\n"
         "          cp-set-ir-detail: unpack=%.2fs norm=%.2fs pack=%.2fs post=%.2fs\n"
         "          sum=%.2fs / wall=%.2fs (%.0f%%)\n",
         g_atp_phase_us_pop_normalize / 1e6,
         100.0 * (g_atp_phase_us_pop_normalize / 1e6) / w,
         g_atp_phase_us_cp_gen / 1e6,
         100.0 * (g_atp_phase_us_cp_gen / 1e6) / w,
         g_atp_phase_us_push_normalize / 1e6,
         100.0 * (g_atp_phase_us_push_normalize / 1e6) / w,
         push_norm_us_per_cp,
         g_atp_phase_us_interreduce / 1e6,
         100.0 * (g_atp_phase_us_interreduce / 1e6) / w,
         g_atp_phase_us_goal_check / 1e6,
         100.0 * (g_atp_phase_us_goal_check / 1e6) / w,
         g_atp_phase_us_cp_set_ir / 1e6,
         100.0 * (g_atp_phase_us_cp_set_ir / 1e6) / w,
         g_atp_phase_us_cpir_unpack / 1e6,
         g_atp_phase_us_cpir_normalize / 1e6,
         g_atp_phase_us_cpir_pack / 1e6,
         g_atp_phase_us_cpir_post / 1e6,
         sumus / 1e6, w,
         100.0 * (sumus / 1e6) / w);
  // The heap-pressure GC fires inside thvm_atp_generate_cps_wm, so its
  // wall is buried in the cp-gen tile above; report it explicitly.
  printf("   gc: collections=%llu total=%.2fs (%.0f%%; inside cp-gen tile)\n",
         (unsigned long long)g_atp_gc_n, g_atp_gc_us / 1e6,
         100.0 * (g_atp_gc_us / 1e6) / w);
  // Incremental goal check (src/atp/goal_incr.c): full normalizations,
  // checks kept with no rule added / after a replay, replays an added
  // rule ended, checks whose fired rules changed, clean-replay misses.
  printf("   goal-incr: full=%llu kept=%llu+%llu diverged=%llu changed=%llu"
         " miss=%llu unorient-rhs-edits=%u\n",
         (unsigned long long)g_atp_goal_incr_full,
         (unsigned long long)g_atp_goal_incr_nonew,
         (unsigned long long)g_atp_goal_incr_replay,
         (unsigned long long)g_atp_goal_incr_diverge,
         (unsigned long long)g_atp_goal_incr_changed,
         (unsigned long long)g_atp_goal_incr_miss, s->unorient_rhs_edits);
  if (g_atp_phase_detail) {
    u64 sumd = g_atp_pushn_us_mirror + g_atp_pushn_us_from +
               g_atp_pushn_us_norm + g_atp_pushn_us_eq +
               g_atp_pushn_us_to + g_atp_pushn_us_ac +
               g_atp_pushn_us_hash + g_atp_pushn_us_gate;
    double per = (g_atp_pushn_calls > 0)
        ? (double)sumd / (double)g_atp_pushn_calls : 0.0;
    printf("   push-norm detail: calls=%llu joined=%llu [%.2fus/call]\n"
           "     mirror=%.2fs from-term=%.2fs norm=%.2fs eq=%.2fs to-term=%.2fs\n"
           "     ac=%.2fs hash=%.2fs size-gate=%.2fs  sum=%.2fs / push-norm=%.2fs\n",
           (unsigned long long)g_atp_pushn_calls,
           (unsigned long long)g_atp_pushn_joined, per,
           g_atp_pushn_us_mirror / 1e6, g_atp_pushn_us_from / 1e6,
           g_atp_pushn_us_norm / 1e6, g_atp_pushn_us_eq / 1e6,
           g_atp_pushn_us_to / 1e6, g_atp_pushn_us_ac / 1e6,
           g_atp_pushn_us_hash / 1e6, g_atp_pushn_us_gate / 1e6,
           sumd / 1e6, g_atp_phase_us_push_normalize / 1e6);
    {
      extern u64 g_acp_pack_nodes_raw, g_acp_pack_nodes_treated,
                 g_acp_pack_n_raw, g_acp_pack_n_treated;
      u64 tot = g_acp_pack_nodes_raw + g_acp_pack_nodes_treated;
      printf("   pack class split: raw>=50 n=%llu nodes=%llu (%.0f%%) | "
             "treated n=%llu nodes=%llu (%.0f%%)\n",
             (unsigned long long)g_acp_pack_n_raw,
             (unsigned long long)g_acp_pack_nodes_raw,
             tot ? 100.0 * (double)g_acp_pack_nodes_raw / (double)tot : 0.0,
             (unsigned long long)g_acp_pack_n_treated,
             (unsigned long long)g_acp_pack_nodes_treated,
             tot ? 100.0 * (double)g_acp_pack_nodes_treated / (double)tot
                 : 0.0);
    }
    {
      extern u64 g_vcp_emit_n_ok, g_vcp_emit_n_fb;
      extern u64 g_vcp_treat_n_ok, g_vcp_treat_n_fb, g_vcp_treat_n_pkfb;
      extern u64 g_vcp_gate_n_test, g_vcp_gate_n_drop, g_vcp_gate_n_fb;
      if (g_vcp_emit_n_ok + g_vcp_emit_n_fb +
          g_vcp_treat_n_ok + g_vcp_treat_n_fb > 0) {
        printf("   ft-emit: raw ok=%llu fb=%llu | treat ok=%llu fb=%llu "
               "pkfb=%llu | gate test=%llu drop=%llu fb=%llu\n",
               (unsigned long long)g_vcp_emit_n_ok,
               (unsigned long long)g_vcp_emit_n_fb,
               (unsigned long long)g_vcp_treat_n_ok,
               (unsigned long long)g_vcp_treat_n_fb,
               (unsigned long long)g_vcp_treat_n_pkfb,
               (unsigned long long)g_vcp_gate_n_test,
               (unsigned long long)g_vcp_gate_n_drop,
               (unsigned long long)g_vcp_gate_n_fb);
      }
      extern u64 g_vcp_lb_hit, g_vcp_lb_raw;
      if (g_vcp_lb_raw > 0) {
        printf("   ft-emit lb probe: hit=%llu of raw=%llu (%.1f%%)\n",
               (unsigned long long)g_vcp_lb_hit,
               (unsigned long long)g_vcp_lb_raw,
               100.0 * (double)g_vcp_lb_hit / (double)g_vcp_lb_raw);
      }
      // later-25 per-CP walk-count attribution: node-visits and walk
      // invocations for the three composite traversals CP formation runs.
      extern u64 g_vcp_cnt_calls, g_vcp_cnt_nodes;
      extern u64 g_vcp_emt_calls, g_vcp_emt_nodes;
      extern u64 g_vcp_bem_calls, g_vcp_bem_nodes;
      if (g_vcp_cnt_calls + g_vcp_emt_calls + g_vcp_bem_calls > 0) {
        printf("   ft-emit walks: count calls=%llu nodes=%llu (%.1f/call) | "
               "raw-emit calls=%llu nodes=%llu (%.1f/call) | "
               "treat-bemit calls=%llu nodes=%llu (%.1f/call)\n",
               (unsigned long long)g_vcp_cnt_calls,
               (unsigned long long)g_vcp_cnt_nodes,
               g_vcp_cnt_calls ? (double)g_vcp_cnt_nodes / (double)g_vcp_cnt_calls : 0.0,
               (unsigned long long)g_vcp_emt_calls,
               (unsigned long long)g_vcp_emt_nodes,
               g_vcp_emt_calls ? (double)g_vcp_emt_nodes / (double)g_vcp_emt_calls : 0.0,
               (unsigned long long)g_vcp_bem_calls,
               (unsigned long long)g_vcp_bem_nodes,
               g_vcp_bem_calls ? (double)g_vcp_bem_nodes / (double)g_vcp_bem_calls : 0.0);
      }
      // later-25 heap-Term interchange volume (dual-rep round-trips).
      extern u64 g_ft_from_calls, g_ft_from_nodes;
      extern u64 g_ft_to_calls, g_ft_to_nodes;
      extern u64 g_acp_unterm_nodes;
      if (g_ft_from_calls + g_ft_to_calls + g_acp_unterm_nodes > 0) {
        printf("   interchange: ft_from_term calls=%llu nodes=%llu (%.1f/call) | "
               "ft_to_term calls=%llu nodes=%llu (%.1f/call) | "
               "acp_unpack_term nodes=%llu\n",
               (unsigned long long)g_ft_from_calls,
               (unsigned long long)g_ft_from_nodes,
               g_ft_from_calls ? (double)g_ft_from_nodes / (double)g_ft_from_calls : 0.0,
               (unsigned long long)g_ft_to_calls,
               (unsigned long long)g_ft_to_nodes,
               g_ft_to_calls ? (double)g_ft_to_nodes / (double)g_ft_to_calls : 0.0,
               (unsigned long long)g_acp_unterm_nodes);
        extern u64 g_ft_from_by_region[8], g_ft_to_by_region[8];
        static const char *reg_name[7] =
            { "other", "pop-norm", "interreduce", "cp-gen", "goal",
              "orient", "drain" };
        printf("   interchange by region:");
        for (int rg = 0; rg < 7; rg++) {
          printf(" %s=%llu/%llu", reg_name[rg],
                 (unsigned long long)g_ft_from_by_region[rg],
                 (unsigned long long)g_ft_to_by_region[rg]);
        }
        printf("  [from/to]\n");
      }
    }
    printf("   push-norm core shape: queries=%llu (var=%llu) cand=%llu "
           "att=%llu hit=%llu\n"
           "     pb=%llu win=%llu rematch=%llu splices=%llu "
           "[cand/q=%.2f att/q=%.2f hit/att=%.3f]\n"
           "     dt-nodes=%llu dt-branch-edges=%llu [node/q=%.2f]\n",
           (unsigned long long)g_atp_pushn_tr_q,
           (unsigned long long)g_atp_pushn_tr_var,
           (unsigned long long)g_atp_pushn_tr_cand,
           (unsigned long long)g_atp_pushn_tr_att,
           (unsigned long long)g_atp_pushn_tr_hit,
           (unsigned long long)g_atp_pushn_tr_pb,
           (unsigned long long)g_atp_pushn_tr_win,
           (unsigned long long)g_atp_pushn_tr_rematch,
           (unsigned long long)g_atp_pushn_rw,
           g_atp_pushn_tr_q ? (double)g_atp_pushn_tr_cand /
                              (double)g_atp_pushn_tr_q : 0.0,
           g_atp_pushn_tr_q ? (double)g_atp_pushn_tr_att /
                              (double)g_atp_pushn_tr_q : 0.0,
           g_atp_pushn_tr_att ? (double)g_atp_pushn_tr_hit /
                                (double)g_atp_pushn_tr_att : 0.0,
           (unsigned long long)g_atp_pushn_dt_node,
           (unsigned long long)g_atp_pushn_dt_edge,
           g_atp_pushn_tr_q ? (double)g_atp_pushn_dt_node /
                              (double)g_atp_pushn_tr_q : 0.0);
    {
      // PHASE-0 allocs-per-rewrite (push scope): gross persistent
      // cells allocated / freed by norm-core splices, split into
      // regime-c (allocating) vs regime-a (in-place) rewrites.  Native
      // WM rewrites the flatterm in place at ~0 alloc/rewrite; this is
      // the direct thvm-vs-native ALLOCATION-axis measurement.
      extern u64 g_atp_pushn_alloc_cells, g_atp_pushn_free_cells;
      extern u64 g_atp_pushn_splice_c, g_atp_pushn_splice_a;
      u64 spl = g_atp_pushn_splice_c + g_atp_pushn_splice_a;
      printf("   push-norm alloc: splices=%llu (regC=%llu regA=%llu) "
             "alloc-cells=%llu free-cells=%llu\n"
             "     [alloc/rw=%.3f free/rw=%.3f regC-frac=%.3f]\n",
             (unsigned long long)spl,
             (unsigned long long)g_atp_pushn_splice_c,
             (unsigned long long)g_atp_pushn_splice_a,
             (unsigned long long)g_atp_pushn_alloc_cells,
             (unsigned long long)g_atp_pushn_free_cells,
             spl ? (double)g_atp_pushn_alloc_cells / (double)spl : 0.0,
             spl ? (double)g_atp_pushn_free_cells / (double)spl : 0.0,
             spl ? (double)g_atp_pushn_splice_c / (double)spl : 0.0);
    }
    if (g_atp_pushn_dt_node > 0) {
      // Deep descend split + per-depth visit histogram + mixmost
      // re-query provenance (see the g_atp_pushn_dt_* block in _.c).
      printf("     dt-split: chain=%llu branch=%llu prune=%llu emit=%llu\n"
             "       bind1=%llu fteq=%llu fteq-ok=%llu fteq-cells=%llu "
             "cprobe=%llu\n",
             (unsigned long long)g_atp_pushn_dt_chain,
             (unsigned long long)g_atp_pushn_dt_branch,
             (unsigned long long)g_atp_pushn_dt_prune,
             (unsigned long long)g_atp_pushn_dt_emit,
             (unsigned long long)g_atp_pushn_dt_bind1,
             (unsigned long long)g_atp_pushn_dt_fteq,
             (unsigned long long)g_atp_pushn_dt_fteq_ok,
             (unsigned long long)g_atp_pushn_dt_fteq_cell,
             (unsigned long long)g_atp_pushn_dt_cprobe);
      printf("     dt-depth-visits:");
      for (int hd = 0; hd < ATP_DT_HIST_N; hd++) {
        if (g_atp_pushn_dt_hist[hd] == 0) continue;
        printf(" %d:%llu", hd,
               (unsigned long long)g_atp_pushn_dt_hist[hd]);
      }
      printf("\n");
      printf("     mm-requery: fix=%llu asc=%llu (of q=%llu)\n",
             (unsigned long long)g_atp_pushn_mm_fix,
             (unsigned long long)g_atp_pushn_mm_asc,
             (unsigned long long)g_atp_pushn_tr_q);
      // Norm-core waste attribution (see the g_atp_pushn_dt_fteq_*
      // block in _.c): repeat-compare decision tiers + walk-site
      // split, var/concrete edge-iteration split, branch fan-out.
      u64 fulleq = g_atp_pushn_dt_fteq - g_atp_pushn_dt_fteq_symfail;
      printf("     dt-fteq-split: chain=%llu branch=%llu | "
             "symfail=%llu (%.1f%%) fulleq=%llu (ok=%llu fail=%llu)\n",
             (unsigned long long)g_atp_pushn_dt_fteq_chain,
             (unsigned long long)(g_atp_pushn_dt_fteq -
                                  g_atp_pushn_dt_fteq_chain),
             (unsigned long long)g_atp_pushn_dt_fteq_symfail,
             g_atp_pushn_dt_fteq
                 ? 100.0 * (double)g_atp_pushn_dt_fteq_symfail /
                       (double)g_atp_pushn_dt_fteq
                 : 0.0,
             (unsigned long long)fulleq,
             (unsigned long long)g_atp_pushn_dt_fteq_ok,
             (unsigned long long)(fulleq - g_atp_pushn_dt_fteq_ok));
      printf("     dt-edge-iters: var=%llu cprobe=%llu "
             "skipped=%llu (of dt-edge=%llu)\n",
             (unsigned long long)g_atp_pushn_dt_vedge,
             (unsigned long long)g_atp_pushn_dt_cprobe,
             (unsigned long long)(g_atp_pushn_dt_edge -
                                  g_atp_pushn_dt_vedge -
                                  g_atp_pushn_dt_cprobe),
             (unsigned long long)g_atp_pushn_dt_edge);
      printf("     dt-branch-fanout (n_edge: visits/edges):");
      for (int fb = 0; fb < ATP_DT_FAN_N; fb++) {
        if (g_atp_pushn_dt_fan_visit[fb] == 0) continue;
        printf(" %d%s:%llu/%llu", fb,
               (fb == ATP_DT_FAN_N - 1) ? "+" : "",
               (unsigned long long)g_atp_pushn_dt_fan_visit[fb],
               (unsigned long long)g_atp_pushn_dt_fan_edge[fb]);
      }
      printf("\n");
      atp_ftdt_hot_dump();
    }
    {
      extern u64 g_vcp_lb_hit, g_vcp_lb_raw;
      if (g_vcp_lb_raw > 0)
        printf("   vcp raw-class lower-bound probe: lb-catches=%llu / raw=%llu [%.1f%%]\n",
               (unsigned long long)g_vcp_lb_hit,
               (unsigned long long)g_vcp_lb_raw,
               100.0 * (double)g_vcp_lb_hit / (double)g_vcp_lb_raw);
    }
    if (g_atp_ftnfm_probe > 0) {
      printf("   ftnfm subtree-NF-memo: probe=%llu hit=%llu [%.1f%%] "
             "store=%llu toolong=%llu reval=%llu reval-ok=%llu\n",
             (unsigned long long)g_atp_ftnfm_probe,
             (unsigned long long)g_atp_ftnfm_hit,
             100.0 * (double)g_atp_ftnfm_hit / (double)g_atp_ftnfm_probe,
             (unsigned long long)g_atp_ftnfm_store,
             (unsigned long long)g_atp_ftnfm_toolong,
             (unsigned long long)g_atp_ftnfm_reval,
             (unsigned long long)g_atp_ftnfm_reval_ok);
    }
    if (g_atp_swrn_hit + g_atp_swrn_miss > 0) {
      printf("   walk rename cache: hit=%llu miss=%llu [%.1f%%]\n",
             (unsigned long long)g_atp_swrn_hit,
             (unsigned long long)g_atp_swrn_miss,
             100.0 * (double)g_atp_swrn_hit /
                 (double)(g_atp_swrn_hit + g_atp_swrn_miss));
    }
    if (g_atp_wmo_rc_hit + g_atp_wmo_rc_miss > 0) {
      printf("   wmo rank cache: hit=%llu miss=%llu [%.1f%%]\n",
             (unsigned long long)g_atp_wmo_rc_hit,
             (unsigned long long)g_atp_wmo_rc_miss,
             100.0 * (double)g_atp_wmo_rc_hit /
                 (double)(g_atp_wmo_rc_hit + g_atp_wmo_rc_miss));
    }
    // CP-gen non-push split (single-walk former regions).  The
    // top-level components (collect/overlap/rank/pushloop) tile the
    // cp-gen phase up to loop overhead; the pushloop's own split
    // (push-norm + varnorm + filters + trace + pack) tiles the
    // pushloop the same way.  kbo is a sub-slice of pack.
    u64 sumg = g_atp_cpg_us_collect + g_atp_cpg_us_overlap +
               g_atp_cpg_us_rank + g_atp_cpg_us_pushloop;
    u64 sump = g_atp_phase_us_push_normalize + g_atp_cpg_us_varnorm +
               g_atp_cpg_us_filters + g_atp_cpg_us_trace +
               g_atp_cpg_us_pack;
    printf("   cp-gen detail: overlap-calls=%llu rank-calls=%llu\n"
           "     collect=%.2fs overlap=%.2fs rank=%.2fs pushloop=%.2fs\n"
           "     sum=%.2fs / cp-gen=%.2fs\n"
           "   pushloop detail: push-norm=%.2fs varnorm=%.2fs "
           "filters=%.2fs trace=%.2fs pack=%.2fs (kbo=%.2fs)\n"
           "     sum=%.2fs / pushloop=%.2fs\n",
           (unsigned long long)g_atp_cpg_overlap_calls,
           (unsigned long long)g_atp_cpg_rank_calls,
           g_atp_cpg_us_collect / 1e6, g_atp_cpg_us_overlap / 1e6,
           g_atp_cpg_us_rank / 1e6, g_atp_cpg_us_pushloop / 1e6,
           sumg / 1e6, g_atp_phase_us_cp_gen / 1e6,
           g_atp_phase_us_push_normalize / 1e6,
           g_atp_cpg_us_varnorm / 1e6, g_atp_cpg_us_filters / 1e6,
           g_atp_cpg_us_trace / 1e6, g_atp_cpg_us_pack / 1e6,
           g_atp_cpg_us_kbo / 1e6,
           sump / 1e6, g_atp_cpg_us_pushloop / 1e6);
    if (g_acp_fuse_n_ok + g_acp_fuse_n_fb > 0) {
      printf("   fused pack: ok=%llu fallback=%llu\n",
             (unsigned long long)g_acp_fuse_n_ok,
             (unsigned long long)g_acp_fuse_n_fb);
    }
  }
  if (g_atp_unorient_step_calls > 0) {
    printf("   unorient-step: %llu calls  %llu fires  %llu empty (%.0f%% wasted)  %.2fs total\n",
           (unsigned long long)g_atp_unorient_step_calls,
           (unsigned long long)g_atp_unorient_step_fires,
           (unsigned long long)g_atp_unorient_step_empty,
           100.0 * (double)g_atp_unorient_step_empty /
                   (double)g_atp_unorient_step_calls,
           g_atp_unorient_step_us / 1e6);
  }
  if (g_atp_wmcp_full > 0) {
    printf("   wm-cp-cand: %llu candidates / %llu full-scan partners "
           "(%.1f%% visited, prune %.1fx)\n",
           (unsigned long long)g_atp_wmcp_cand,
           (unsigned long long)g_atp_wmcp_full,
           100.0 * (double)g_atp_wmcp_cand / (double)g_atp_wmcp_full,
           (double)g_atp_wmcp_full / (double)(g_atp_wmcp_cand ? g_atp_wmcp_cand : 1));
    printf("   wm-cp-overlap: %llu zero-yield / %llu nonzero-yield pairs "
           "(%.1f%% wasted overlaps)\n",
           (unsigned long long)g_atp_wmcp_zero,
           (unsigned long long)g_atp_wmcp_nonzero,
           100.0 * (double)g_atp_wmcp_zero /
                   (double)((g_atp_wmcp_zero + g_atp_wmcp_nonzero)
                            ? (g_atp_wmcp_zero + g_atp_wmcp_nonzero) : 1));
  }
  {
    u64 unf_total = g_atp_unf_memo_hits + g_atp_unf_memo_misses;
    if (unf_total > 0) {
      printf("   unf-memo: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
             (unsigned long long)g_atp_unf_memo_hits,
             (unsigned long long)unf_total,
             100.0 * (double)g_atp_unf_memo_hits / (double)unf_total);
    }
    u64 si_total = g_atp_ri_splice_inline_hits + g_atp_ri_splice_inline_misses;
    if (si_total > 0) {
      printf("   splice-inline: %llu hits / %llu attempts (%.0f%% hit-ratio)\n",
             (unsigned long long)g_atp_ri_splice_inline_hits,
             (unsigned long long)si_total,
             100.0 * (double)g_atp_ri_splice_inline_hits / (double)si_total);
    }
    u64 pm_total = g_atp_unf_pos_memo_hits + g_atp_unf_pos_memo_misses;
    if (pm_total > 0) {
      printf("   pos-memo: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
             (unsigned long long)g_atp_unf_pos_memo_hits,
             (unsigned long long)pm_total,
             100.0 * (double)g_atp_unf_pos_memo_hits / (double)pm_total);
    }
    u64 rc_total = g_atp_rhs_cache_hits + g_atp_rhs_cache_misses;
    if (rc_total > 0) {
      printf("   rhs-flat-cache: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
             (unsigned long long)g_atp_rhs_cache_hits,
             (unsigned long long)rc_total,
             100.0 * (double)g_atp_rhs_cache_hits / (double)rc_total);
    }
    u64 nc_total = g_atp_norm_cache_hits + g_atp_norm_cache_misses;
    if (nc_total > 0) {
      printf("   norm-cache: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
             (unsigned long long)g_atp_norm_cache_hits,
             (unsigned long long)nc_total,
             100.0 * (double)g_atp_norm_cache_hits / (double)nc_total);
    }
    u64 jc_total = g_atp_join_cache_hits + g_atp_join_cache_misses;
    if (jc_total > 0) {
      printf("   join-cache: %llu hits / %llu queries (%.0f%% hit-ratio)\n",
             (unsigned long long)g_atp_join_cache_hits,
             (unsigned long long)jc_total,
             100.0 * (double)g_atp_join_cache_hits / (double)jc_total);
    }
  }
}
