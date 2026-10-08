// I18N-01 fixture, not compiled: stands in for the tr("...") calls that arrive
// with I18N-02, so the bundled vi.tsv seed is checked against a real scan.
// Drop it (and its use in test_i18n01_catalog.cpp) once src/ carries the keys.
void seed()
{
    tr("Open...");
    tr("Save");
    tr("Move %d of %d");
    tr("Open a .rdb file");
    tr("Free Renju");
    tr("Analysis stopped after {n} moves");
}
