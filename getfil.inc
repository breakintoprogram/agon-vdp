_mos_getfil:
	push	ix
	ld 		ix,0
	add 	ix, sp
	ld 		bc, (ix+6)	; File identifier
	ld a,	mos_getfil
	rst.lil	08h			; Get a pointer to the relevant FIL struct
	ld		sp,ix
	pop		ix
	ret