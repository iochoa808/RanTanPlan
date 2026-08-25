;; SEMANTIC BREAK: write value has a type that is incompatible with the
;; array's declared element type.
;;
;; SEMANTIC RULE: The value written into an array cell must be assignable
;; to the cell's element type.
;;
;; Case 1 — object written into a numeric array:
;;   `int_arr` is (array 3 val) where val = (number 0 4).
;;   `(write (int_arr) (?i) item_a)` — item_a is an object constant,
;;   not a number.  Writing an object reference to a numeric cell is a
;;   type error: the array cannot hold object values.
;;
;; Case 2 — integer written into an object array:
;;   `obj_arr` is (array 3 token) where token is an object type.
;;   `(write (obj_arr) (?i) 5)` — 5 is an integer literal.  Object-typed
;;   array cells store object references (encoded as indices), not raw
;;   integers.  Writing a bare integer to an object cell is a type error.
;;
;; These cross-type writes should be caught at semantic analysis time.
;; Silently encoding item_a as an integer (e.g., its object index) or 5
;; as an object reference would produce undefined behaviour.
;;
;; Expected: type error for `cross_write_obj` and `cross_write_int`.

(define (domain X-sem-write-wrong-elem-type)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        val   - (number 0 4)
        token - object

        int_store - (array 3 val)     ; element type: bounded integer
        obj_store - (array 3 token)   ; element type: object
    )

    (:constants tok_a tok_b tok_c item_a - token)

    (:functions
        (int_arr) - int_store
        (obj_arr) - obj_store
    )

    ;; item_a is an object; int_arr expects numeric elements — type mismatch
    (:action cross_write_obj
        :parameters (?i - slot)
        :precondition ()
        :effect (write (int_arr) (?i) item_a)
    )

    ;; 5 is an integer; obj_arr expects token objects — type mismatch
    (:action cross_write_int
        :parameters (?i - slot)
        :precondition ()
        :effect (write (obj_arr) (?i) 5)
    )

    ;; Correct writes — for contrast.
    (:action write_int_ok
        :parameters (?i - slot ?v - val)
        :precondition ()
        :effect (write (int_arr) (?i) ?v)
    )

    (:action write_obj_ok
        :parameters (?i - slot ?t - token)
        :precondition ()
        :effect (write (obj_arr) (?i) ?t)
    )
)
