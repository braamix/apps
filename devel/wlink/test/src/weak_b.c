// Strong definitions that win over weak_a.c's, whatever the order.
const char *weak_who(void)
{
    return "strong";
}

int weak_value = 2;
