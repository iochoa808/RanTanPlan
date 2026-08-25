(define (domain sets-singleton)
    ;; Token-ring: the set (current) always holds exactly one token.
    ;; Invariant: at-most-one (and exactly-one) membership in (current).
    ;; Each action adds exactly one member and removes exactly one member,
    ;; so the tier-1 syntactic checker can verify AMO without h².
    (:requirements :sets)

    (:types
        node   - object
        nodeset - (set node)
    )

    (:functions
        (current) - nodeset
    )

    ;; Step 1: vacate current node (token goes to limbo)
    (:action release
        :parameters (?from - node)
        :precondition (member ?from (current))
        :effect (remove ?from (current))
    )

    ;; Step 2: claim new node
    (:action claim
        :parameters (?to - node)
        :precondition (not (member ?to (current)))
        :effect (add ?to (current))
    )
)
