;; PDDL-XTS translation of pddl/test/woodworking .../problem.pddl (simple-wood-prob).
;; Attribute predicates become object-fluent assignments. The unused part p0 is
;; given placeholder attribute values (overwritten when it is cut) so its object
;; fluents are initialised.
(define (problem simple-wood-prob-xts)
    (:domain woodworking-xts)
    (:objects
        grinder0 - grinder
        immersion-varnisher0 - immersion-varnisher
        highspeed-saw0 - highspeed-saw
        mauve - acolour
        beech - awood
        p0 - part
        b0 - board
        s0 s1 s2 - aboardsize
    )
    (:init
        (grind-treatment-change varnished colourfragments)
        (grind-treatment-change glazed untreated)
        (grind-treatment-change untreated untreated)
        (grind-treatment-change colourfragments untreated)
        (is-smooth smooth) (is-smooth verysmooth)
        (boardsize-successor s0 s1) (boardsize-successor s1 s2)
        (has-colour immersion-varnisher0 mauve)
        (empty highspeed-saw0)
        (unused p0)
        (= (goalsize-of p0) small)
        ;; placeholder attributes for the unused part (overwritten by cut):
        (= (surface-of p0) smooth) (= (wood-of p0) beech)
        (= (treatment-of p0) untreated) (= (colour-of p0) natural)
        ;; board b0:
        (= (boardsize-of b0) s2) (= (wood-of b0) beech) (= (surface-of b0) smooth)
        (available b0)
    )
    (:goal (and (available p0) (= (colour-of p0) mauve)
                (= (wood-of p0) beech) (= (treatment-of p0) varnished)))
)
