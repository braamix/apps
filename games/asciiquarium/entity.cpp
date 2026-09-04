// entity.go: the entity model, with CallbackArgs written out as fields.

#include "quarium.h"

// Split once here, not by strings.Split on every draw. Empty is one empty line.
bool Frame::assign(Str text)
{
    lines.clear();
    for (;;) {
        usize i  = text.find('\n');
        Str head = i == Str::npos ? text : text.substr(0, i);
        String s;
        if (!s.assign(head) || !lines.push(static_cast<String &&>(s)))
            return false;
        if (i == Str::npos)
            break;
        text = text.substr(i + 1);
    }
    return true;
}

// new_entity leaves at least one frame in each, as NewEntity's normalisation
// of an empty Shape slice does.
const Frame &Entity::current_shape() const
{
    return shapes[usize(current_frame) % shapes.size()];
}

const Frame &Entity::current_color() const
{
    return colors[usize(current_frame) % colors.size()];
}

// Frame zero's box, and never recomputed: a multi-frame entity keeps it.
void Entity::size_from_first_frame()
{
    height = 0;
    width  = 0;
    if (shapes.empty())
        return;
    const Frame &f = shapes[0];
    height         = i32(f.lines.size());
    for (const String &ln : f.lines)
        if (i32(ln.size()) > width)
            width = i32(ln.size());
}

// The default movement. frame_step accumulates until it reaches one.
bool Entity::move_entity()
{
    if (mode == HOOK_NONE) {
        x += dx;
        y += dy;
        z += dz;
        if (frame_step > 0) {
            frame_time += frame_step;
            if (frame_time >= 1) {
                current_frame++;
                frame_time = 0;
            }
            frame_count++;
        }
    } else if (shapes.size() > 1) {
        // The type switch's other arm: mode arguments animate at a tenth.
        frame_time += 0.1;
        if (frame_time >= 1) {
            current_frame++;
            frame_time = 0;
            frame_count++;
        }
    }
    return true;
}

// Killed by hand, out of frames, or off the screen.
bool Entity::should_die(i32 screen_width, i32 screen_height) const
{
    if (!alive)
        return true;
    if (die_frame > 0 && frame_count >= die_frame)
        return true;
    if (die_offscreen) {
        if (x + f64(width) < 0 || x >= f64(screen_width) || y + f64(height) < 0 ||
            y >= f64(screen_height))
            return true;
    }
    return false;
}

// Move, then react to hits. The collision list is the previous frame's, which
// is upstream's order and is visible.
void Entity::update(Animation *anim)
{
    if (callback)
        callback(this, anim);
    else
        move_entity();
    if (coll_handler && !collision.empty())
        coll_handler(this, anim);
}
