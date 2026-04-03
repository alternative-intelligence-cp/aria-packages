# aria-decision-t — Decision Tree

**Version:** 0.2.0  
**Type:** pure Aria  
**Ported:** v0.10.1

## API Reference

```aria
// Decision outcomes
dt_none() → int64
dt_unknown() → int64
dt_a() → int64
dt_b() → int64
dt_both() → int64

// Trigger codes
dt_trig_manual() → int64
dt_trig_init() → int64
dt_trig_lean_a() → int64
dt_trig_lean_unkn() → int64
dt_trig_lean_b() → int64
dt_trig_tilt_both() → int64
dt_trig_tilt_unkn() → int64
dt_trig_tilt_none() → int64
dt_trig_observe() → int64

// Axis labels
dt_axis_a() → int64
dt_axis_b() → int64

// Decision graph operations
dg_opt_a(pbc: flt64, pbm: flt64, width: flt64) → flt64     // optimal A score
dg_opt_b(pbc: flt64, pbm: flt64, width: flt64) → flt64     // optimal B score
dg_decision_code(pbc: flt64, pbm: flt64, width: flt64) → int64  // decision code
dg_create(dg: wild flt64->, width: flt64) → NIL             // create decision graph
dg_lean_a(dg: wild flt64->, amount: flt64) → NIL            // lean toward A
dg_lean_unkn(dg: wild flt64->, amount: flt64) → NIL         // lean toward unknown
dg_lean_b(dg: wild flt64->, amount: flt64) → NIL            // lean toward B
dg_tilt_both(dg: wild flt64->, amount: flt64) → NIL         // tilt toward both
dg_tilt_unkn(dg: wild flt64->, amount: flt64) → NIL         // tilt toward unknown
dg_tilt_none(dg: wild flt64->, amount: flt64) → NIL         // tilt toward none
dg_pbc(dg: wild flt64->) → flt64                            // get P(both|certain)
dg_pbm(dg: wild flt64->) → flt64                            // get P(both|maybe)
dg_width(dg: wild flt64->) → flt64                          // get width
dgt_snapshot(dg: wild flt64->, meta: wild int64->, snaps: wild flt64->, trigger_code: int64) → NIL
```

## Dependencies

None (pure Aria).
