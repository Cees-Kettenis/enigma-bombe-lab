# English scoring data

The no-crib search uses a bundled table of 17,576 English trigram scores in
`src/english_trigrams.inc`. It makes no runtime network requests. Python is only
needed to regenerate the table, not to build or run the application.

The counts come from two public-domain novels, distributed by Project Gutenberg:

- Jane Austen, *Pride and Prejudice*, https://www.gutenberg.org/ebooks/1342
- Mary Shelley, *Frankenstein*, https://www.gutenberg.org/ebooks/84

Downloaded on 2026-09-23. The generated table records the SHA-256 of each input.
The generator strips the Gutenberg header/footer, uppercases the text and keeps
ASCII A-Z only. It counts overlapping triples within each book, adds 0.1 to each
count and stores natural-log probabilities multiplied by 1000 as signed 16-bit
integers. The two books are not concatenated across their boundary.

To reproduce, download the corresponding UTF-8 texts and run:

```sh
python3 tools/train_english.py /path/to/1342.txt /path/to/84.txt
```

The application averages these log scores. A higher score means the candidate
resembles this English corpus more closely. It is not a probability that the key
or plaintext is correct. Literary English is a limited model for military text,
names, abbreviations, German, random strings, and corrupted messages.

The original prose in `tests/test_blind.c` and `examples/no-crib.ini` is excluded
from the training corpus. The example uses an early rotor state so you can see a
result quickly. It does not measure arbitrary-key recovery time or success rate.

## Confidence display

All candidate rows, in both crib and no-crib modes, use this same text-only
trigram model. The display converts the average log score `s` to a heuristic
English confidence index:

```text
100 / (1 + exp(-2.3555511833 * (s + 8.75)))
```

The fixed display anchors are 5% at -10, 50% at -8.75, and 95% at -7.5.
These are chosen scale anchors, not empirical recovery rates or likelihoods.
Finite results are limited to 0.1–99.9%, so the display never claims certainty.
Sorting retains the underlying score to distinguish percentages that round alike.
Changing, removing, or revealing a stored challenge cannot change a candidate's
confidence. Known-answer comparisons are absent from candidate processing.

This index describes resemblance to the English corpus. It is not a posterior
probability that a decryption is right. Fluent wrong text, short samples, unusual
language and the many hypotheses explored by a search can all mislead it.
