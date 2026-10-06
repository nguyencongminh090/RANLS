# Instruction — ARCH-05

Detail: [docs/todo/ARCH-05-extract-game-file-service.md](../todo/ARCH-05-extract-game-file-service.md)

## Approach

Pure move of the GTK-free statements out of the two dialog callbacks; keep the dialog callbacks as glue. Pick the layer so rule 8 holds (add an include-guard check if it lands in a new directory).

## Pitfalls

Error strings shown in `showErrorDialog` must stay identical. The post-load `controller_.sendConfig()` stays in `MainWindow`. Save must still create no writer for non-`.rdb`.

## Verification before done

`RUN_TESTS=1 ./build.sh` green; RDB tests unchanged.

## Boundaries

No format or UI change.
