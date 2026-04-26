;; Refinery domain (extended).
;;
;; A refinery is brought online once (setup), then can be cycled through
;; shifts: start-operating occupies one of (max-active) operator slots
;; and produces output equal to capacity + calibration; send-to-consolidator
;; pushes the output into the shared combined-oil pool and clears the
;; refinery's output (so it can be operated again); stop-operating frees
;; the operator slot. Refining is two-stage: stage1-refine consumes
;; combined-oil and produces an intermediate per product; stage2-refine
;; consumes the intermediate to deliver the product. Stage 1 actions
;; serialize on combined-oil; stage 2 actions parallelize per product.
;;
;; Two structural properties matter for RantanPlan:
;;
;; 1. Goal-relevance BFS reachability. Every step
;;       setup -> start-operating -> stop-operating
;;                                -> send-to-consolidator -> stage1-refine
;;                                -> stage2-refine
;;    is gated by an explicit Boolean precondition or numeric guard
;;    that the BFS can follow. The full chain survives pruning.
;;
;; 2. Value-expression cascade hook. The auxiliary action calibrate(?r)
;;    sets calibration(?r), which is read only inside start-operating's
;;    effect value `(+ (capacity ?r) (calibration ?r))`. No precondition
;;    or goal references calibration, so the BFS cannot reach calibrate.
;;    Iteration 1 prunes all calibrate(?r); iteration 2's RPG sees
;;    calibration(?r) collapse to 0 and downstream output and
;;    combined-oil bounds tighten; achievers re-verification fires.
;;
;; The plan structure with this domain typically consists of multiple
;; shifts of (start, send, stop) interleaved with stage1-refine actions,
;; followed by a final parallelizable round of stage2-refine. With
;; tight demand and small max-active the planner has to actually
;; schedule shifts rather than parallelizing everything at once.

(define (domain refinery)
  (:requirements :strips :typing :numeric-fluents)
  (:types refinery product)

  (:predicates
    (online ?r - refinery)
    (operating ?r - refinery)
    (has-output ?r - refinery)
    (stage1-done ?p - product)
    (delivered ?p - product))

  (:functions
    (output ?r - refinery)
    (capacity ?r - refinery)         ; static
    (calibration ?r - refinery)      ; AUX: BFS-blind cascade hook
    (combined-oil)
    (intermediate-amount ?p - product)
    (demand ?p - product)            ; static
    (active-count)
    (max-active))                    ; static

  (:action setup
    :parameters (?r - refinery)
    :precondition (not (online ?r))
    :effect (online ?r))

  ;; AUX: BFS-blind. Pruned in iter 1.
  (:action calibrate
    :parameters (?r - refinery)
    :effect (assign (calibration ?r) 10))

  ;; Begin a shift on a refinery: occupies an operator slot,
  ;; commits its output to the refinery's local buffer.
  (:action start-operating
    :parameters (?r - refinery)
    :precondition (and (online ?r)
                       (not (operating ?r))
                       (not (has-output ?r))
                       (< (active-count) (max-active)))
    :effect (and (operating ?r)
                 (has-output ?r)
                 (increase (active-count) 1)
                 (assign (output ?r) (+ (capacity ?r) (calibration ?r)))))

  ;; End the shift, free the operator slot. Output remains buffered
  ;; until send-to-consolidator runs.
  (:action stop-operating
    :parameters (?r - refinery)
    :precondition (operating ?r)
    :effect (and (not (operating ?r))
                 (decrease (active-count) 1)))

  ;; Push buffered output into the shared combined-oil pool.
  ;; Clears has-output so the refinery can be operated again.
  (:action send-to-consolidator
    :parameters (?r - refinery)
    :precondition (has-output ?r)
    :effect (and (not (has-output ?r))
                 (increase (combined-oil) (output ?r))
                 (assign (output ?r) 0)))

  ;; Stage 1 refining: distill product-specific intermediate from
  ;; combined-oil. Costs the product's demand from combined-oil.
  (:action stage1-refine
    :parameters (?p - product)
    :precondition (and (not (stage1-done ?p))
                       (>= (combined-oil) (demand ?p)))
    :effect (and (stage1-done ?p)
                 (decrease (combined-oil) (demand ?p))
                 (assign (intermediate-amount ?p) (demand ?p))))

  ;; Stage 2 refining: final processing of the intermediate into the
  ;; delivered product. Independent across products (parallelizable).
  (:action stage2-refine
    :parameters (?p - product)
    :precondition (and (stage1-done ?p)
                       (>= (intermediate-amount ?p) (demand ?p))
                       (not (delivered ?p)))
    :effect (and (delivered ?p)
                 (decrease (intermediate-amount ?p) (demand ?p))))
)
