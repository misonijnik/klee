## Get source code

Our version of KLEE can be found at [link](https://github.com/UnitTestBot/klee/commits/luban-utbot-main).

## Build

There is a step-by-step [guide](https://klee.github.io/build-llvm11/) how to build KLEE by original authors.

## Run

The following flags should be used when calling klee binary.

### Essential flags

| KLEE flag | Description |
|-----------|-------------|
| --execution-mode=error-guided | Enables symbolic execution guidance by input errors from off-the-shelf static analyzer |
| --mock-external-calls | All external calls will be treated symbolically |
| --posix-runtime | POSIX environment is emulated |
| --libc=klee | **To UtBotTeam: why not uclibc?** |
| --skip-not-lazy-and-symbolic-pointers | Lazy initialization optimization; crucial for performance |
| --analysis-reproduce=/analyzer/result.json | Path of JSON file containing static analysis paths to be reproduced |

### Nonessential flags

| KLEE flag | Description |
|-----------|-------------|
| --output-dir=/path/to/some/folder.klee-out | Directory in which to write results. Note that it’s parent folder must exist (e.g., a folder /path/to/some must exist) and this folder must not exist (e.g., a folder /path/to/some/folder.klee-out must not exist). |

#### Nonessential flags to control quality/time

| KLEE flag | Description |
|-----------|-------------|
| --max-depth=N | Only allow N symbolic branches. Set to 0 to disable. |
| --max-time=Ns | Halt execution after N seconds. Set to 0s to disable. |
| --max-solver-time=Ns | N seconds is a maximum amount of time for a single SMT query. Set to 0s to disable. |
| --max-instructions=N | Stop execution after N instructions. Set to 0 to disable. |
| --max-forks=N | Only fork N times. Set to -1 to disable. |
| --max-stack-frames=N | Terminate a symbolic state after N stack frames in symbolic state. Set to 0 to disable. |

### Our KLEE reports results to **stderr** as lines of the form:

`KLEE: WARNING: False Negative at: *filename*:*error line*:*error column*`

where *filename*, *error line*, *error column* is an error address with program trace which does not match input source-sink request, which is proven to be reachable.

`KLEE: WARNING: *verdict* at trace *number*`

where

* *verdict* is either: “False Positive” or “True Positive”
* *number* is an id of the proved or refuted trace from JSON input

### Known limitations

Current version supports only `null dereference` type of errors. So, it does not support, e.g., `reverse null` type of errors.

## Examples

All example JSON inputs for `--analysis-reproduce` flag can be found [here](test/Industry).

Examples can be run automatically with the following command

```bash
~/klee/build $ lit -a test/Industry
```
