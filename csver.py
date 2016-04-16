import sys

class Exp:
    def __init__(self):
        self.key = {}
        self.value = {}

    def _record_key_val_pair(self, dikt, k, v):
        if not dikt.has_key(k):
            dikt[k] = v
        else:
            raise Exception("Repeated key: This case should not be reached!")

    def record_key(self, k, v):
        self._record_key_val_pair(self.key, k, v)

    def record_value(self, k, v):
        self._record_key_val_pair(self.value, k, v)

    def record_value_from_line(self, k, line):
        self.record_value(k, line.split()[-2])

exps = {}
for fname in sys.argv[1:]:
    exp = Exp()
    with open(fname, "r") as f:
        line = f.readline()                          # Called with: argc=7
        while line:
            assert line.startswith("Called")
            line = f.readline().split()              # ./ko -m 1024 -n 65536 -k 8
            for i in [1,3,5]:
                exp.record_key(line[i][1], line[i+1])
            while True:
                line = f.readline()                  # ...
                if line.startswith("Total time:"):   # Total time:  75.109 ms
                    exp.record_value_from_line("tt", line)
                elif line.startswith("Time/multp:"): # Time/multp:   0.001 ms
                    exp.record_value_from_line("tpm", line)
                elif line.startswith("Time/round:"): # Time/round:   0.009 ms
                    exp.record_value_from_line("tpr", line)
                elif line.startswith("Ops/second:"): # Ops/second:   0.873 mmps
                    exp.record_value_from_line("mmps", line)
                elif not line:
                    break
                else:
                    continue

    exps[tuple(exp.key.items())] = exp.value.items()

columns=["m","n","k","tt","tpm","tpr","mmps"]
print ','.join(columns)
for k in sorted(exps):
    d = dict(list(k) + exps[k])
    print ','.join([d[c] for c in columns])
