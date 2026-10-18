# Contrapunctus Universalis

(WIP) An engine for generating perpetual counterpoint: four voices with continuous frequencies evolve under a minimal set of physical laws. The counterpoint itself (contrary motion, voice crossings, fusions) is not coded — it emerges from the constraints.

## Project status

Under development. Design document finalized. Implementation in C (C11) in progress.

## Principle

Four oscillators (`double`, unit: hertz) share a continuous common register. No scale, no temperament, no coded interval system: the frequency space is continuous, and the system is not bound to any tonal tradition. The laws are constraints only:

| # | Law | Effect |
|---|-----|--------|
| 1 | Positivity | no frequency ≤ 0 |
| 2 | Independence | two voices may never occupy the same frequency |
| 3 | Proximity repulsion | voices too close repel each other; the threshold, in hertz, is a law on beat period |
| 4 | Register bounds | If a voice is getting to high in register, it goes down, if a voice goes too low in register, it goes up. |
| 5 | Temporal bounds | If a pitch is emitted for too long, it stops. The pitch must be emitted for at least one tick. |

## License

GPL-3.0 — see [LICENSE](LICENSE).

# Soli Contrapunctus Gloria
