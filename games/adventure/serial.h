// serial.h: the save file.  Ser writes, De reads, and Game::visit() names every
// field once, so the two cannot drift apart.
//
// Little-endian 16-bit words throughout; an i32 is two of them, low first.
// travel_ is not in the file -- rdata() rebuilds it before restore() runs, and
// nothing in play changes it.
#pragma once

#include "hdr.h"

constexpr Str SAVE_MAGIC = "ADVSAVE\002"; // format 2: fields, not a struct

struct Ser {
    String out;
    bool ok = true;

    void field(u16 &v) { put(v); }
    void field(i16 &v) { put(u16(v)); }
    void field(i32 &v)
    {
        put(u16(u32(v)));
        put(u16(u32(v) >> 16));
    }
    void field(Text &t) { field(t.seekadr), field(t.txtlen); }
    void field(HashTab &h) { field(h.val), field(h.hash); }

    template <class T, usize N>
    void field(T (&a)[N])
    {
        for (usize i = 0; i < N; i++)
            field(a[i]);
    }

    template <class T>
    void field(Vec<T> &v)
    {
        i32 n = i32(v.size());
        field(n);
        for (usize i = 0; i < v.size(); i++)
            field(v[i]);
    }

private:
    void put(u16 x)
    {
        char b[2] = { char(x), char(x >> 8) };
        ok        = ok && out.append(Str(b, 2));
    }
};

struct De {
    Str in;
    usize pos = 0;
    bool ok   = true;

    void field(u16 &v) { v = get(); }
    void field(i16 &v) { v = i16(get()); }
    void field(i32 &v)
    {
        u16 lo = get();
        u16 hi = get();
        v      = i32(u32(lo) | u32(hi) << 16);
    }
    void field(Text &t) { field(t.seekadr), field(t.txtlen); }
    void field(HashTab &h) { field(h.val), field(h.hash); }

    template <class T, usize N>
    void field(T (&a)[N])
    {
        for (usize i = 0; i < N; i++)
            field(a[i]);
    }

    // alloc() has already sized every Vec, so a count that disagrees is a bad
    // file -- and refusing it is what keeps every index unchecked afterwards.
    template <class T>
    void field(Vec<T> &v)
    {
        i32 n = 0;
        field(n);
        if (!ok || n < 0 || usize(n) != v.size()) {
            ok = false;
            return;
        }
        for (usize i = 0; i < v.size(); i++)
            field(v[i]);
    }

private:
    u16 get()
    {
        if (pos + 2 > in.size()) {
            ok = false;
            return 0;
        }
        u16 x = u16(u8(in[pos])) | u16(u16(u8(in[pos + 1])) << 8);
        pos += 2;
        return x;
    }
};

// Upstream's struct game, field by field.  What glorkz decides and play never
// changes is left out.
template <class V>
void Game::visit(V &v)
{
    v.field(loc_);
    v.field(newloc_);
    v.field(oldloc_);
    v.field(oldlc2_);
    v.field(wzdark_);
    v.field(gaveup_);
    v.field(kq_);
    v.field(k_);
    v.field(k2_);
    v.field(verb_);
    v.field(obj_);
    v.field(spk_);
    v.field(blklin_);
    v.field(saved_);
    v.field(savet_);
    v.field(mxscor_);
    v.field(latncy_);
    v.field(voc_);
    v.field(rtext_);
    v.field(mtext_);
    v.field(clsses_);
    v.field(ctext_);
    v.field(cval_);
    v.field(ptext_);
    v.field(ltext_);
    v.field(stext_);
    v.field(atloc_);
    v.field(plac_);
    v.field(fixd_);
    v.field(fixed_);
    v.field(actspk_);
    v.field(cond_);
    v.field(hntmax_);
    v.field(hints_);
    v.field(hinted_);
    v.field(hintlc_);
    v.field(place_);
    v.field(prop_);
    v.field(plink_);
    v.field(abb_);
    v.field(maxtrs_);
    v.field(tally_);
    v.field(tally2_);
    v.field(keys_);
    v.field(lamp_);
    v.field(grate_);
    v.field(cage_);
    v.field(rod_);
    v.field(rod2_);
    v.field(steps_);
    v.field(bird_);
    v.field(door_);
    v.field(pillow_);
    v.field(snake_);
    v.field(fissur_);
    v.field(tablet_);
    v.field(clam_);
    v.field(oyster_);
    v.field(magzin_);
    v.field(dwarf_);
    v.field(knife_);
    v.field(food_);
    v.field(bottle_);
    v.field(water_);
    v.field(oil_);
    v.field(plant_);
    v.field(plant2_);
    v.field(axe_);
    v.field(mirror_);
    v.field(dragon_);
    v.field(chasm_);
    v.field(troll_);
    v.field(troll2_);
    v.field(bear_);
    v.field(messag_);
    v.field(vend_);
    v.field(batter_);
    v.field(nugget_);
    v.field(coins_);
    v.field(chest_);
    v.field(eggs_);
    v.field(tridnt_);
    v.field(vase_);
    v.field(emrald_);
    v.field(pyram_);
    v.field(pearl_);
    v.field(rug_);
    v.field(chain_);
    v.field(spices_);
    v.field(back_);
    v.field(look_);
    v.field(cave_);
    v.field(null_);
    v.field(entrnc_);
    v.field(dprssn_);
    v.field(say_);
    v.field(lock_);
    v.field(throw_);
    v.field(find_);
    v.field(invent_);
    v.field(chloc_);
    v.field(chloc2_);
    v.field(dseen_);
    v.field(dloc_);
    v.field(odloc_);
    v.field(dflag_);
    v.field(daltlc_);
    v.field(tk_);
    v.field(stick_);
    v.field(dtotal_);
    v.field(attack_);
    v.field(turns_);
    v.field(lmwarn_);
    v.field(iwest_);
    v.field(knfloc_);
    v.field(detail_);
    v.field(abbnum_);
    v.field(maxdie_);
    v.field(numdie_);
    v.field(holdng_);
    v.field(dkill_);
    v.field(foobar_);
    v.field(bonus_);
    v.field(clock1_);
    v.field(clock2_);
    v.field(closng_);
    v.field(panic_);
    v.field(closed_);
    v.field(scorng_);
    v.field(limit_);
}
