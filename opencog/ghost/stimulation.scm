; ----------
; ECAN related configuration

; Update some of the parameters
(State (Concept "AF_RENT_FREQUENCY") (Number 0.5))
(State (Concept "MAX_SPREAD_PERCENTAGE") (Number 0.8))
(State (Concept "HEBBIAN_MAX_ALLOCATION_PERCENTAGE") (Number 1))

(define default-stimulus 150)

; Stimulate words when doing 'nlp-parse'
(nlp-start-stimulation default-stimulus)

; ----------
(define-public (ghost-set-default-stimulus STIMULUS)
"
  Change the default stimulus to the given value.
"
  (if (number? STIMULUS)
    (set! default-stimulus STIMULUS)
    (cog-logger-warn ghost-logger "Stimulus should be a number!"))
)

(define-public (ghost-stimulate . ATOMS)
"
  Stimulate the given list of atoms with the default stimulus.
"
  (map (lambda (a) (cog-stimulate a default-stimulus)) ATOMS)
)

