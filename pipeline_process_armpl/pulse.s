	.arch armv8-a
	.file	"pulse.c"
	.text
	.align	2
	.p2align 4,,11
	.global	transpose_rd_pulse_range_to_doppler_range_pulse
	.type	transpose_rd_pulse_range_to_doppler_range_pulse, %function
transpose_rd_pulse_range_to_doppler_range_pulse:
.LFB54:
	.cfi_startproc
	sub	sp, sp, #96
	.cfi_def_cfa_offset 96
	adrp	x4, :got:__stack_chk_guard
	ldr	x4, [x4, :got_lo12:__stack_chk_guard]
	stp	x29, x30, [sp, 48]
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	add	x29, sp, 48
	stp	x19, x20, [sp, 64]
	.cfi_offset 19, -32
	.cfi_offset 20, -24
	mov	x20, x2
	mov	x19, x3
	stp	x21, x22, [sp, 80]
	.cfi_offset 21, -16
	.cfi_offset 22, -8
	mov	x22, x0
	mov	x21, x1
	ldr	x0, [x4]
	str	x0, [sp, 40]
	mov	x0, 0
	add	x1, sp, 8
	mov	w0, 1
	bl	clock_gettime
	cbz	x22, .L18
	ldr	x15, [x22, 8]
	cmp	x21, 0
	ccmp	x15, 0, 4, ne
	beq	.L18
	ldr	x30, [x21, 8]
	cmp	x20, 0
	ccmp	x30, 0, 4, ne
	beq	.L18
	ldp	w11, w17, [x20, 20]
	cmp	w17, 0
	ble	.L3
	lsl	w20, w11, 4
	lsl	w14, w17, 4
	sbfiz	x10, x11, 3, 32
	ubfiz	x4, x17, 3, 32
	mov	w18, 0
	mov	x13, 0
.L4:
	mov	w16, w13
	cmp	w11, 0
	ble	.L8
	add	w0, w13, 15
	add	w9, w13, 16
	cmp	w17, w0
	add	x12, x30, w18, sxtw 3
	csel	w9, w9, w17, gt
	mov	w21, 0
	mov	x7, 0
	.p2align 3,,7
.L7:
	add	w0, w7, 15
	add	w8, w7, 16
	cmp	w11, w0
	csel	w8, w8, w11, gt
	cmp	w9, w13
	ble	.L11
	add	x5, x13, w21, sxtw
	sub	w3, w8, w7
	mov	x2, x12
	mov	w6, w16
	add	x5, x15, x5, lsl 3
	.p2align 3,,7
.L10:
	mov	x1, x5
	mov	x0, 0
	cmp	w8, w7
	ble	.L13
	.p2align 3,,7
.L12:
	ldr	d0, [x1]
	add	x1, x1, x4
	str	d0, [x2, x0, lsl 3]
	add	x0, x0, 1
	cmp	x0, x3
	bne	.L12
.L13:
	add	w6, w6, 1
	add	x5, x5, 8
	add	x2, x2, x10
	cmp	w9, w6
	bne	.L10
.L11:
	add	x7, x7, 16
	add	x12, x12, 128
	add	w21, w21, w14
	cmp	w11, w7
	bgt	.L7
.L8:
	add	x13, x13, 16
	add	w18, w18, w20
	cmp	w17, w13
	bgt	.L4
.L3:
	add	x1, sp, 24
	mov	w0, 1
	bl	clock_gettime
	mov	x1, 145685290680320
	movk	x1, 0x412e, lsl 48
	fmov	d1, x1
	ldp	x3, x2, [sp, 8]
	ldp	x1, x0, [sp, 24]
	sub	x0, x0, x2
	sub	x1, x1, x3
	mov	x2, 70368744177664
	scvtf	d0, x0
	movk	x2, 0x408f, lsl 48
	fmov	d2, x2
	mov	w0, 0
	fdiv	d0, d0, d1
	scvtf	d1, x1
	fmadd	d0, d1, d2, d0
	str	d0, [x19]
.L1:
	adrp	x1, :got:__stack_chk_guard
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]
	ldr	x3, [sp, 40]
	ldr	x2, [x1]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L25
	ldp	x29, x30, [sp, 48]
	ldp	x19, x20, [sp, 64]
	ldp	x21, x22, [sp, 80]
	add	sp, sp, 96
	.cfi_remember_state
	.cfi_restore 29
	.cfi_restore 30
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
.L18:
	.cfi_restore_state
	mov	w0, -1
	b	.L1
.L25:
	bl	__stack_chk_fail
	.cfi_endproc
.LFE54:
	.size	transpose_rd_pulse_range_to_doppler_range_pulse, .-transpose_rd_pulse_range_to_doppler_range_pulse
	.align	2
	.p2align 4,,11
	.global	make_pulse_compression_filter
	.type	make_pulse_compression_filter, %function
make_pulse_compression_filter:
.LFB58:
	.cfi_startproc
	sub	sp, sp, #192
	.cfi_def_cfa_offset 192
	adrp	x3, :got:__stack_chk_guard
	ldr	x3, [x3, :got_lo12:__stack_chk_guard]
	stp	x29, x30, [sp, 48]
	.cfi_offset 29, -144
	.cfi_offset 30, -136
	add	x29, sp, 48
	ldp	s2, s1, [x0, 12]
	stp	d8, d9, [sp, 144]
	.cfi_offset 72, -48
	.cfi_offset 73, -40
	ldr	s9, [x0, 4]
	stp	x23, x24, [sp, 96]
	.cfi_offset 23, -96
	.cfi_offset 24, -88
	mov	w23, w1
	mov	x24, x2
	fdiv	s8, s1, s2
	fmul	s0, s9, s2
	stp	x25, x26, [sp, 112]
	ldr	x1, [x3]
	str	x1, [sp, 40]
	mov	x1, 0
	stp	xzr, xzr, [sp, 24]
	fcvt	d0, s0
	.cfi_offset 25, -80
	.cfi_offset 26, -72
	bl	llround
	cmp	w0, 0
	ble	.L28
	stp	x27, x28, [sp, 128]
	.cfi_offset 28, -56
	.cfi_offset 27, -64
	add	x27, sp, 24
	mov	x2, x27
	mov	w1, 1
	stp	x19, x20, [sp, 64]
	.cfi_offset 20, -120
	.cfi_offset 19, -128
	stp	x21, x22, [sp, 80]
	.cfi_offset 22, -104
	.cfi_offset 21, -112
	mov	w22, w0
	bl	alloc_complex_matrix
	mov	w19, w0
	cbnz	w0, .L68
	adrp	x0, .LC0
	fcvt	d8, s8
	ldrsw	x21, [sp, 28]
	add	x26, sp, 8
	ldr	d0, [x0, #:lo12:.LC0]
	mov	x25, sp
	ldr	x20, [sp, 32]
	stp	d10, d11, [sp, 160]
	.cfi_offset 75, -24
	.cfi_offset 74, -32
	fmul	d8, d8, d0
	movi	d10, #0
	lsl	x21, x21, 3
	.p2align 3,,7
.L30:
	scvtf	s1, w19
	mov	x1, x25
	mov	x0, x26
	add	w19, w19, 1
	fdiv	s1, s1, s9
	fcvt	d1, s1
	fmul	d0, d8, d1
	fmul	d0, d0, d1
	fcvt	s0, d0
	fcvt	d0, s0
	bl	sincos
	ldp	d0, d1, [sp]
	fmadd	d0, d1, d10, d0
	fcvt	s1, d1
	fcvt	s0, d0
	stp	s0, s1, [x20]
	add	x20, x20, x21
	cmp	w22, w19
	bne	.L30
	ldr	w20, [sp, 24]
	mov	x2, x24
	mov	w1, 1
	mov	w0, w20
	bl	alloc_complex_matrix
	mov	w26, w0
	mov	x0, x27
	cbnz	w26, .L70
	cbnz	w23, .L71
	mov	x22, 0
	cmp	w20, 0
	ble	.L50
.L44:
	ldrsw	x4, [sp, 28]
	sub	w0, w20, #1
	ldr	x3, [sp, 32]
	sxtw	x0, w0
	lsl	x1, x4, 3
	ldrsw	x5, [x24, 4]
	ldr	x2, [x24, 8]
	neg	x4, x4, lsl 3
	madd	x1, x1, x0, x3
	lsl	x5, x5, 3
	add	x0, x22, x0, lsl 2
	b	.L49
	.p2align 2,,3
.L55:
	mov	x0, x3
.L49:
	ldp	s1, s0, [x1]
	cbz	w23, .L48
	ldr	s2, [x0]
	fmul	s1, s1, s2
	fmul	s0, s0, s2
.L48:
	fneg	s0, s0
	add	x1, x1, x4
	sub	x3, x0, #4
	stp	s1, s0, [x2]
	add	x2, x2, x5
	cmp	x0, x22
	bne	.L55
.L50:
	mov	x0, x22
	bl	free
	mov	x0, x27
	bl	free_complex_matrix
	ldp	x19, x20, [sp, 64]
	.cfi_restore 20
	.cfi_restore 19
	ldp	x21, x22, [sp, 80]
	.cfi_restore 22
	.cfi_restore 21
	ldp	x27, x28, [sp, 128]
	.cfi_restore 28
	.cfi_restore 27
	ldp	d10, d11, [sp, 160]
	.cfi_restore 75
	.cfi_restore 74
.L26:
	adrp	x0, :got:__stack_chk_guard
	ldr	x0, [x0, :got_lo12:__stack_chk_guard]
	ldr	x2, [sp, 40]
	ldr	x1, [x0]
	subs	x2, x2, x1
	mov	x1, 0
	bne	.L72
	ldp	x29, x30, [sp, 48]
	mov	w0, w26
	ldp	x23, x24, [sp, 96]
	ldp	x25, x26, [sp, 112]
	ldp	d8, d9, [sp, 144]
	add	sp, sp, 192
	.cfi_restore 29
	.cfi_restore 30
	.cfi_restore 25
	.cfi_restore 26
	.cfi_restore 23
	.cfi_restore 24
	.cfi_restore 72
	.cfi_restore 73
	.cfi_def_cfa_offset 0
	ret
	.p2align 2,,3
.L71:
	.cfi_def_cfa_offset 192
	.cfi_offset 19, -128
	.cfi_offset 20, -120
	.cfi_offset 21, -112
	.cfi_offset 22, -104
	.cfi_offset 23, -96
	.cfi_offset 24, -88
	.cfi_offset 25, -80
	.cfi_offset 26, -72
	.cfi_offset 27, -64
	.cfi_offset 28, -56
	.cfi_offset 29, -144
	.cfi_offset 30, -136
	.cfi_offset 72, -48
	.cfi_offset 73, -40
	.cfi_offset 74, -32
	.cfi_offset 75, -24
	sxtw	x25, w20
	mov	x1, 4
	mov	x0, x25
	bl	calloc
	mov	x22, x0
	cbz	x0, .L69
	cmp	w20, 0
	ble	.L34
	mov	x1, 4
	mov	x0, x1
	bl	calloc
	mov	x21, x0
	cbz	x0, .L34
	adrp	x0, .LC1
	mov	x1, 1
	fmov	d6, 1.0e+0
	fmov	d10, 5.0e-1
	ldr	d9, [x0, #:lo12:.LC1]
	adrp	x0, .LC2
	fmov	d11, -1.0e+0
	stp	d12, d13, [sp, 176]
	.cfi_offset 77, -8
	.cfi_offset 76, -16
	ldr	d8, [x0, #:lo12:.LC2]
	.p2align 3,,7
.L35:
	fmov	d4, 1.0e+0
	mul	w0, w1, w1
	mov	w2, w1
	fmov	s7, w0
	fmov	d5, d4
	mov	w3, 1
	.p2align 3,,7
.L37:
	cmp	w3, w1
	beq	.L36
	scvtf	s1, w3
	scvtf	s0, s7
	mul	w0, w3, w3
	fcvt	d1, s1
	scvtf	s3, w0
	fcvt	d2, s0
	fdiv	s0, s0, s3
	fsub	d1, d1, d10
	fmadd	d1, d1, d1, d9
	fmul	d1, d1, d8
	fcvt	d0, s0
	fdiv	d1, d2, d1
	fsub	d0, d6, d0
	fmul	d0, d0, d5
	fcvt	s0, d0
	fcvt	d5, s0
	fsub	d0, d6, d1
	fmul	d0, d0, d4
	fcvt	s0, d0
	fcvt	d4, s0
.L36:
	add	w3, w3, 1
	cmp	w3, 4
	bne	.L37
	fadd	d5, d5, d5
	add	w2, w2, 1
	tst	x2, 1
	fdiv	d4, d4, d5
	fcsel	d0, d6, d11, eq
	fmul	d4, d4, d0
	fcvt	s4, d4
	str	s4, [x21, x1, lsl 2]
	add	x1, x1, 1
	cmp	x1, 4
	bne	.L35
	scvtf	s11, w20
	fmov	d12, 1.0e+0
	fmov	d0, 5.0e-1
	adrp	x0, .LC3
	movi	v10.2s, #0
	mov	x28, 0
	ldr	d9, [x0, #:lo12:.LC3]
	fcvt	d11, s11
	fsub	d12, d11, d12
	fmul	d12, d12, d0
	.p2align 3,,7
.L42:
	scvtf	s8, w28
	mov	x19, 1
	fmov	s13, 1.0e+0
	fcvt	d8, s8
	fsub	d8, d8, d12
	fdiv	d8, d8, d11
	fcvt	s8, d8
	fcvt	d8, s8
	.p2align 3,,7
.L40:
	scvtf	d0, w19
	fmul	d0, d0, d9
	fmul	d0, d0, d8
	bl	cos
	fcvt	d13, s13
	ldr	s1, [x21, x19, lsl 2]
	add	x19, x19, 1
	fcvt	d1, s1
	fadd	d1, d1, d1
	fmadd	d13, d1, d0, d13
	fcvt	s13, d13
	cmp	x19, 4
	bne	.L40
	fcmpe	s10, s13
	str	s13, [x22, x28, lsl 2]
	bmi	.L53
.L41:
	add	x28, x28, 1
	cmp	x25, x28
	bne	.L42
	fcmpe	s10, #0.0
	bgt	.L43
	mov	x0, x21
	bl	free
	ldp	d12, d13, [sp, 176]
	.cfi_remember_state
	.cfi_restore 77
	.cfi_restore 76
	b	.L44
	.p2align 2,,3
.L53:
	.cfi_restore_state
	fmov	s10, s13
	b	.L41
.L43:
	add	x25, x22, x25, lsl 2
	mov	x0, x22
	.p2align 3,,7
.L45:
	ldr	s0, [x0]
	fdiv	s0, s0, s10
	str	s0, [x0], 4
	cmp	x25, x0
	bne	.L45
	mov	x0, x21
	bl	free
	ldp	d12, d13, [sp, 176]
	.cfi_restore 77
	.cfi_restore 76
	b	.L44
.L68:
	.cfi_restore 74
	.cfi_restore 75
	ldp	x19, x20, [sp, 64]
	.cfi_restore 20
	.cfi_restore 19
	ldp	x21, x22, [sp, 80]
	.cfi_restore 22
	.cfi_restore 21
	ldp	x27, x28, [sp, 128]
	.cfi_restore 28
	.cfi_restore 27
.L28:
	mov	w26, -1
	b	.L26
	.p2align 2,,3
.L34:
	.cfi_offset 19, -128
	.cfi_offset 20, -120
	.cfi_offset 21, -112
	.cfi_offset 22, -104
	.cfi_offset 27, -64
	.cfi_offset 28, -56
	.cfi_offset 74, -32
	.cfi_offset 75, -24
	mov	x0, x22
	bl	free
.L69:
	mov	x0, x27
	bl	free_complex_matrix
	mov	x0, x24
.L70:
	bl	free_complex_matrix
	mov	w26, -1
	ldp	x19, x20, [sp, 64]
	.cfi_restore 20
	.cfi_restore 19
	ldp	x21, x22, [sp, 80]
	.cfi_restore 22
	.cfi_restore 21
	ldp	x27, x28, [sp, 128]
	.cfi_restore 28
	.cfi_restore 27
	ldp	d10, d11, [sp, 160]
	.cfi_restore 75
	.cfi_restore 74
	b	.L26
.L72:
	stp	x19, x20, [sp, 64]
	.cfi_offset 20, -120
	.cfi_offset 19, -128
	stp	x21, x22, [sp, 80]
	.cfi_offset 22, -104
	.cfi_offset 21, -112
	stp	x27, x28, [sp, 128]
	.cfi_offset 28, -56
	.cfi_offset 27, -64
	stp	d10, d11, [sp, 160]
	.cfi_offset 75, -24
	.cfi_offset 74, -32
	stp	d12, d13, [sp, 176]
	.cfi_offset 77, -8
	.cfi_offset 76, -16
	bl	__stack_chk_fail
	.cfi_endproc
.LFE58:
	.size	make_pulse_compression_filter, .-make_pulse_compression_filter
	.align	2
	.p2align 4,,11
	.global	pulse_compress_ctx_destroy
	.type	pulse_compress_ctx_destroy, %function
pulse_compress_ctx_destroy:
.LFB61:
	.cfi_startproc
	cbz	x0, .L99
	stp	x29, x30, [sp, -32]!
	.cfi_def_cfa_offset 32
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	mov	x29, sp
	str	x19, [sp, 16]
	.cfi_offset 19, -16
	mov	x19, x0
	ldr	x0, [x0, 64]
	cbz	x0, .L75
	bl	fftwf_destroy_plan
.L75:
	ldr	x0, [x19, 72]
	cbz	x0, .L76
	bl	fftwf_destroy_plan
.L76:
	ldr	x0, [x19, 40]
	cbz	x0, .L77
	bl	fftwf_free
.L77:
	ldr	x0, [x19, 48]
	cbz	x0, .L78
	bl	fftwf_free
.L78:
	ldr	x0, [x19, 56]
	cbz	x0, .L79
	bl	fftwf_free
.L79:
	mov	x0, x19
	bl	free_complex_matrix
	movi	v0.4s, 0
	stp	q0, q0, [x19]
	stp	q0, q0, [x19, 32]
	str	q0, [x19, 64]
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 32
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 19
	.cfi_def_cfa_offset 0
	ret
	.p2align 2,,3
.L99:
	ret
	.cfi_endproc
.LFE61:
	.size	pulse_compress_ctx_destroy, .-pulse_compress_ctx_destroy
	.align	2
	.p2align 4,,11
	.global	pulse_compress_ctx_init
	.type	pulse_compress_ctx_init, %function
pulse_compress_ctx_init:
.LFB60:
	.cfi_startproc
	stp	x29, x30, [sp, -48]!
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	cmp	x0, 0
	ccmp	x1, 0, 4, ne
	mov	x29, sp
	stp	x19, x20, [sp, 16]
	.cfi_offset 19, -32
	.cfi_offset 20, -24
	beq	.L104
	movi	v0.4s, 0
	mov	x19, x1
	mov	x2, x1
	mov	w1, 1
	stp	q0, q0, [x19]
	stp	q0, q0, [x19, 32]
	str	q0, [x19, 64]
	ldr	w3, [x0, 24]
	str	w3, [x19, 16]
	bl	make_pulse_compression_filter
	mov	w20, w0
	cbnz	w0, .L104
	ldr	w3, [x19]
	ldr	w2, [x19, 16]
	add	w2, w3, w2
	sub	w2, w2, #1
	stp	w3, w2, [x19, 20]
	cmp	w2, 1
	ble	.L118
	mov	w1, 1
	.p2align 3,,7
.L107:
	lsl	w1, w1, 1
	cmp	w2, w1
	bgt	.L107
	sbfiz	x0, x1, 3, 32
.L106:
	sub	w3, w3, #1
	stp	w1, w3, [x19, 28]
	bl	fftwf_malloc
	str	x0, [x19, 40]
	ldrsw	x1, [x19, 28]
	lsl	x0, x1, 3
	bl	fftwf_malloc
	str	x0, [x19, 48]
	ldrsw	x1, [x19, 28]
	lsl	x0, x1, 3
	bl	fftwf_malloc
	mov	x1, x0
	ldr	x0, [x19, 40]
	str	x1, [x19, 56]
	cbz	x0, .L108
	ldr	x2, [x19, 48]
	cmp	x2, 0
	ccmp	x1, 0, 4, ne
	beq	.L108
	ldrsw	x2, [x19, 28]
	mov	w1, 0
	lsl	x2, x2, 3
	bl	memset
	ldr	x0, [x19, 48]
	mov	w1, 0
	ldrsw	x2, [x19, 28]
	lsl	x2, x2, 3
	bl	memset
	ldr	x0, [x19, 56]
	mov	w1, 0
	ldrsw	x2, [x19, 28]
	lsl	x2, x2, 3
	bl	memset
	ldr	x2, [x19, 48]
	mov	w4, 0
	ldr	w0, [x19, 28]
	mov	w3, -1
	mov	x1, x2
	bl	fftwf_plan_dft_1d
	mov	x1, x0
	ldr	x2, [x19, 56]
	str	x1, [x19, 64]
	ldr	w0, [x19, 28]
	mov	w4, 0
	mov	x1, x2
	mov	w3, 1
	bl	fftwf_plan_dft_1d
	str	x0, [x19, 72]
	ldr	x1, [x19, 64]
	cmp	x1, 0
	ccmp	x0, 0, 4, ne
	beq	.L108
	ldr	w0, [x19, 20]
	ldr	x2, [x19, 40]
	str	x21, [sp, 32]
	.cfi_offset 21, -16
	cmp	w0, 0
	ble	.L113
	ldrsw	x4, [x19, 4]
	add	x0, x2, w0, sxtw 3
	ldr	x3, [x19, 8]
	mov	x1, x2
	lsl	x4, x4, 3
	.p2align 3,,7
.L112:
	ldr	d0, [x3]
	add	x3, x3, x4
	str	d0, [x1], 8
	cmp	x1, x0
	bne	.L112
.L113:
	ldr	w0, [x19, 28]
	mov	x1, x2
	mov	w4, 0
	mov	w3, -1
	bl	fftwf_plan_dft_1d
	mov	x21, x0
	cbz	x0, .L131
	bl	fftwf_execute
	mov	x0, x21
	bl	fftwf_destroy_plan
	ldr	w2, [x19, 28]
	fmov	s1, 1.0e+0
	scvtf	s0, w2
	fdiv	s1, s1, s0
	dup	v1.2s, v1.s[0]
	cmp	w2, 0
	ble	.L130
	ldr	x1, [x19, 40]
	add	x2, x1, w2, sxtw 3
	.p2align 3,,7
.L116:
	ldr	d0, [x1]
	fmul	v0.2s, v0.2s, v1.2s
	str	d0, [x1], 8
	cmp	x1, x2
	bne	.L116
.L130:
	ldr	x21, [sp, 32]
	.cfi_restore 21
.L102:
	mov	w0, w20
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 48
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
	.p2align 2,,3
.L118:
	.cfi_restore_state
	mov	x0, 8
	mov	w1, 1
	b	.L106
.L131:
	.cfi_offset 21, -16
	ldr	x21, [sp, 32]
	.cfi_restore 21
.L108:
	mov	x0, x19
	bl	pulse_compress_ctx_destroy
.L104:
	mov	w20, -1
	b	.L102
	.cfi_endproc
.LFE60:
	.size	pulse_compress_ctx_init, .-pulse_compress_ctx_init
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC4:
	.string	"pulse_compress_one: ctx is NULL\n"
	.align	3
.LC5:
	.string	"pulse_compress_one: raw_pulse is NULL\n"
	.align	3
.LC6:
	.string	"pulse_compress_one: out_range_bins is NULL\n"
	.align	3
.LC7:
	.string	"pulse_compress_one: ctx not initialized\n"
	.text
	.align	2
	.p2align 4,,11
	.global	pulse_compress_one
	.type	pulse_compress_one, %function
pulse_compress_one:
.LFB62:
	.cfi_startproc
	sub	sp, sp, #144
	.cfi_def_cfa_offset 144
	adrp	x4, :got:__stack_chk_guard
	ldr	x4, [x4, :got_lo12:__stack_chk_guard]
	stp	x29, x30, [sp, 48]
	.cfi_offset 29, -96
	.cfi_offset 30, -88
	add	x29, sp, 48
	stp	x19, x20, [sp, 64]
	.cfi_offset 19, -80
	.cfi_offset 20, -72
	mov	x19, x1
	add	x1, sp, 8
	stp	x21, x22, [sp, 80]
	.cfi_offset 21, -64
	.cfi_offset 22, -56
	mov	x21, x0
	stp	x25, x26, [sp, 112]
	.cfi_offset 25, -32
	.cfi_offset 26, -24
	mov	x26, x3
	str	x27, [sp, 128]
	.cfi_offset 27, -16
	mov	x27, x2
	ldr	x0, [x4]
	str	x0, [sp, 40]
	mov	x0, 0
	mov	w0, 1
	bl	clock_gettime
	cbz	x21, .L158
	cbz	x19, .L159
	cbz	x27, .L160
	ldr	w2, [x21, 16]
	cmp	w2, 0
	ble	.L138
	ldr	w0, [x21, 28]
	cmp	w0, 0
	ble	.L138
	ldr	x0, [x21, 40]
	cbz	x0, .L138
	ldr	x0, [x21, 48]
	cbz	x0, .L138
	ldr	x1, [x21, 56]
	cbz	x1, .L138
	ldr	x1, [x21, 64]
	cbz	x1, .L138
	ldr	x1, [x21, 72]
	cbz	x1, .L138
	sbfiz	x2, x2, 3, 32
	mov	x1, x19
	bl	memcpy
	ldr	x3, [x21, 48]
	mov	w1, 0
	ldr	w0, [x21, 16]
	ldr	w2, [x21, 28]
	sub	w2, w2, w0
	add	x0, x3, w0, sxtw 3
	sbfiz	x2, x2, 3, 32
	bl	memset
	ldr	x0, [x21, 64]
	bl	fftwf_execute
	ldr	w25, [x21, 28]
	cmp	w25, 0
	ble	.L140
	stp	x23, x24, [sp, 96]
	.cfi_offset 24, -40
	.cfi_offset 23, -48
	sbfiz	x25, x25, 3, 32
	mov	x20, 4
	ldp	x24, x23, [x21, 40]
	mov	x19, 0
	ldr	x22, [x21, 56]
	.p2align 3,,7
.L142:
	ldr	s1, [x23, x20]
	ldr	s2, [x24, x19]
	ldr	s3, [x24, x20]
	ldr	s0, [x23, x19]
	fmul	s4, s2, s1
	fmul	s5, s3, s1
	fmadd	s4, s3, s0, s4
	fnmsub	s5, s2, s0, s5
	fcmp	s4, s5
	bvs	.L161
.L141:
	str	s5, [x22, x19]
	add	x19, x19, 8
	str	s4, [x22, x20]
	add	x20, x20, 8
	cmp	x25, x19
	bne	.L142
	ldp	x23, x24, [sp, 96]
	.cfi_restore 24
	.cfi_restore 23
.L140:
	ldr	x0, [x21, 72]
	bl	fftwf_execute
	ldrsw	x2, [x21, 16]
	mov	x0, x27
	ldr	x1, [x21, 56]
	ldrsw	x3, [x21, 32]
	lsl	x2, x2, 3
	add	x1, x1, x3, lsl 3
	bl	memcpy
	add	x1, sp, 24
	mov	w0, 1
	bl	clock_gettime
	mov	x1, 145685290680320
	movk	x1, 0x412e, lsl 48
	fmov	d1, x1
	ldp	x3, x2, [sp, 8]
	ldp	x1, x0, [sp, 24]
	sub	x0, x0, x2
	sub	x1, x1, x3
	mov	x2, 70368744177664
	scvtf	d0, x0
	movk	x2, 0x408f, lsl 48
	fmov	d2, x2
	mov	w0, 0
	fdiv	d0, d0, d1
	scvtf	d1, x1
	fmadd	d0, d1, d2, d0
	str	d0, [x26]
.L132:
	adrp	x1, :got:__stack_chk_guard
	ldr	x1, [x1, :got_lo12:__stack_chk_guard]
	ldr	x3, [sp, 40]
	ldr	x2, [x1]
	subs	x3, x3, x2
	mov	x2, 0
	bne	.L162
	ldp	x29, x30, [sp, 48]
	ldp	x19, x20, [sp, 64]
	ldp	x21, x22, [sp, 80]
	ldp	x25, x26, [sp, 112]
	ldr	x27, [sp, 128]
	add	sp, sp, 144
	.cfi_remember_state
	.cfi_restore 29
	.cfi_restore 30
	.cfi_restore 27
	.cfi_restore 25
	.cfi_restore 26
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
.L138:
	.cfi_restore_state
	adrp	x3, :got:stderr
	ldr	x3, [x3, :got_lo12:stderr]
	adrp	x0, .LC7
	mov	x2, 40
	add	x0, x0, :lo12:.LC7
	mov	x1, 1
	ldr	x3, [x3]
	bl	fwrite
	mov	w0, -1
	b	.L132
.L161:
	.cfi_offset 23, -48
	.cfi_offset 24, -40
	bl	__mulsc3
	fmov	s5, s0
	fmov	s4, s1
	b	.L141
.L160:
	.cfi_restore 23
	.cfi_restore 24
	adrp	x3, :got:stderr
	ldr	x3, [x3, :got_lo12:stderr]
	adrp	x0, .LC6
	mov	x2, 43
	add	x0, x0, :lo12:.LC6
	mov	x1, 1
	ldr	x3, [x3]
	bl	fwrite
	mov	w0, -1
	b	.L132
.L159:
	adrp	x3, :got:stderr
	ldr	x3, [x3, :got_lo12:stderr]
	adrp	x0, .LC5
	mov	x2, 38
	add	x0, x0, :lo12:.LC5
	mov	x1, 1
	ldr	x3, [x3]
	bl	fwrite
	mov	w0, -1
	b	.L132
.L158:
	adrp	x3, :got:stderr
	ldr	x3, [x3, :got_lo12:stderr]
	adrp	x0, .LC4
	mov	x2, 32
	add	x0, x0, :lo12:.LC4
	mov	x1, 1
	ldr	x3, [x3]
	bl	fwrite
	mov	w0, -1
	b	.L132
.L162:
	stp	x23, x24, [sp, 96]
	.cfi_offset 24, -40
	.cfi_offset 23, -48
	bl	__stack_chk_fail
	.cfi_endproc
.LFE62:
	.size	pulse_compress_one, .-pulse_compress_one
	.section	.rodata.cst8,"aM",@progbits,8
	.align	3
.LC0:
	.word	1413754136
	.word	1074340347
	.align	3
.LC1:
	.word	-2147483648
	.word	1073471598
	.align	3
.LC2:
	.word	-1610612736
	.word	1072843704
	.align	3
.LC3:
	.word	1413754136
	.word	1075388923
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
