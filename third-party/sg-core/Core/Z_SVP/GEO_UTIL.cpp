#include "../sg.h"

/*
	 © ¯®¥ª ®ª p ­  ¯¬, ¯®®¤ ¥¥§ ®ª l1,l2

	®§¢  ¥: NULL - ¥« ®ª l1,l2 ®¢¯ ¤ 
										 ®®¡¥­¥ ­¥ ¢¤ ¥!
							«ª ­  ª®¬ ®ª - ¢ ­®¬ «­®¬ « ¥
	¥®¤:
		ª®¬  ®ª  ­ ®¤ § ¥¥­ ¥¬ § 4- «­.-­©:
			pp = l1 + alpha * (l2 - l1)   - pp «¥¦ ­  ¯¬®©
			(p - pp, l2 - l1) = 0         - ª «­®¥ ¯®§¢. = 0
*/
lpD_POINT projection_on_line(lpD_POINT p,
														 lpD_POINT l1, lpD_POINT l2,
														 lpD_POINT pp)
{
	D_POINT ll;
	sgFloat 	alpha;

	dpoint_sub(l2, l1, &ll);
	alpha = dskalar_product(&ll, &ll);
	if (alpha < eps_d) return NULL;
	alpha = (dskalar_product(&ll, p) - dskalar_product(&ll, l1)) / alpha;
	return dpoint_parametr(l1, l2, alpha, pp);
}

/*
	« ¢¥­ p2  p3 ¯¬®£®«­ª  ¯® § ¤ ­­¬ ¢¥­ ¬
	p0, p1  ¯® ¢¥­¥, «¥¦ ¥© ­  ¯¬®© p2,p3.
	®§¬®¦­  ¢®¦¤¥­­® ¯¬®£®«­ª  ¢ ®¥§®ª p0,p1.

	®§¢  ¥: TRUE  - ¢¥­ p2,p3 ¢«¥­
							FALSE - ¯¬®£®«­ª ¢®¦¤¥­ ¢ ®¥§®ª p0,p1 «
											®¥§®ª p0,p1 ¬¥¥ ­«¥¢ ¤«­
											®®¡¥­¥ ­¥ ¢¤ ¥!
*/
BOOL calc_face4(lpD_POINT p0, lpD_POINT p1, lpD_POINT p,
								lpD_POINT p2, lpD_POINT p3)
{
	D_POINT pp, a;

	if (!projection_on_line(p, p0, p1, &pp)) return FALSE;
	dpoint_sub(p, &pp, &a);
	dpoint_add(p0, &a, p2);
	dpoint_add(p1, &a, p3);
	return TRUE;
}
