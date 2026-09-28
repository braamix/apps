// Weak definitions that come too late: weak_a.c's weak one and weak_b.c's
// strong one are already there, and both must stay.
__attribute__((weak)) const char *weak_only(void)
{
    return "weak too late";
}

__attribute__((weak)) int weak_value = 3;
