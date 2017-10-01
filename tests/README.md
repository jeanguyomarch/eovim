# Eovim Tests

## Foreword

Eovim is a graphical application that uses Neovim as a backend. As such, there
is little cleverness in Eovim itself. It just a graphical front-end. Since we
don't offer an API, unit testing makes little sense, the only thing that really
matters is WYSIWYG.

So we do pixel-perfect tests, using [Exactness][1], a pixel-perfect framework
for [EFL][2] applications. The workflow is the following:

- I run `eovim` in a windowed environment, through `exactness`, do stuff with
  it, take snapshots of what it looks like at some point of this operation
  that is called *recording*.
- At the end of *recording*, `exactness` dumps binary data that can be
  replayed and compared against another run of `eovim`. If the results
  observed are the same than the one previously recorded, test passes,
  otherwise test fails.

Two points shall be noted:

- Exactness does create binary files, possibly quite big because of the
  snapshots. Even though files are compressed, it is dirty to have these
  files in the Eovim repository. That's why they will reside in a
  dedicated (git) repository. It must be manually cloned using
  `scripts/get-test-data.sh` and will be available as `.deps/test-data/`.
- For reproductible tests, the nvim environment as well as the eovim
  configuration should be the same for several executions of one test. This
  is why the `tests/env/` directory provides configurations to be used for
  tests.


## Updating the tests

After having the test-data setup, run the following (assuming you are in the
top source directory and you have built eovim in `build`):

```bash
exactness \
   --dest-dir .deps/test-data/recordings \
   --base-dir .deps/test-data/orig \
   --record build/tests/tests.txt
exactness \
   --dest-dir .deps/test-data/recordings \
   --base-dir .deps/test-data/orig \
   --init build/tests/tests.txt
```

Don't forget to commit and push the modifications to the `test-data`
repository.


## Tests List

The following list describes the tests implemented:

- *test_minimal*: open an empty file, take a snapshot, and close eovim with the
  command `:qa!`.


[1]: https://git.enlightenment.org/tools/exactness.git/
[2]: https://www.enlightenment.org/
