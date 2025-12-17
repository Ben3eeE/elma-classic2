#include "ALL.H"

void editplay(void) {
    invalidateegesz();
    if (Valtozott || State->editor_filename[0] == 0 || Ptop->hibas) {
        if (!tolt_save(0)) {
            return;
        }
    }

    if (Ptop->hibas) {
        return;
    }

    floadlevel_p(State->editor_filename);

    Prec1->erase(State->editor_filename);
    Prec2->erase(State->editor_filename);
    lejatszo(State->editor_filename);

    mk_emptychar();

    // Visszaallitjuk eredeti allapotot:
    Pal_editor->set();

    /*initmou( 640, 480 );

    // Megprobaljuk kozepre hozni egeret:
    for( int i = 0; i < 1000; i++ )
        getmou( &Moux, &Mouy );
    int mmx = 300;
    int mmy = 255;
    Moux = mmx;
    Mouy = mmy;
    setmou( mmx, mmy );
    Moux = mmx;
    Mouy = mmy;*/
}
