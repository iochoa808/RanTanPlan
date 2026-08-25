# Scaling corpus — `bench.py --families scaling`

80 size-parametrized instances across 11 domains, built to measure how the
encoding scales with instance size and to put classical and XTS models of the
*same* problem side by side.

```
generators/<domain>.py            builds a Problem for a given size: generate(n=...)
instances/<domain>/n<N>.py        a 3-line stub pinning one size
```

Each stub adds `../..` to `sys.path` and imports its generator, so the two
directories have to stay siblings:

```python
import sys, os; sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))
from generators.delivery_bool import generate
def get_problem(): return generate(n=4)
```

## The domains

Five of them come in matched pairs — the same problem modelled classically and
with an XTS feature, so a size sweep shows where the XTS encoding starts to win
or lose rather than just how fast one model is:

| pair | classical | XTS |
|---|---|---|
| delivery | `delivery_bool` (10) | `delivery_sets` (5) |
| expedition | `expedition_classic` (6) | `expedition_array` (6) |
| gripper | `gripper_bool` (10) | `gripper_sets` (7) |
| visit-all | `visit_all_bool` (4) | `visit_all_sets` (4) |
| fo-counters | `fo_counters` (7) | `fo_counters_small` (7) — fixed vs scaled ranges |

Plus `pancake` (14), an array-stress sweep with no classical counterpart.

## scaling vs synthetic

`--families synthetic` generates from the same generators but with its own size
lists, computed at run time. `scaling` is the pinned set: the sizes are checked
in, so two runs months apart compare instance-for-instance. Use `scaling` when
you want to compare against an earlier sweep, `synthetic` when you want to push
sizes further than the checked-in list.

## Adding a size

Drop a stub next to the others — no registration step, `bench.py` discovers
`instances/*/*.py`:

```bash
printf "%s\n%s\n%s\n" \
  "import sys, os; sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))" \
  "from generators.pancake import generate" \
  "def get_problem(): return generate(n=16)" \
  > instances/pancake/n16.py
```
