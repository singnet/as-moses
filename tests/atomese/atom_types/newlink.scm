(use-modules (opencog) (opencog exec) (opencog asmoses))

(define test_or
(New (TypeNode "OrLink") (ListLink (Predicate "P") (Predicate "Q"))))

(define test_and
(New (TypeNode "AndLink") (ListLink (Predicate "P") (Predicate "Q"))))

