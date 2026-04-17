
	* = $C000

	LDX	#0
loop:
	STX	$801B
	INX
	BCC	loop
	BCS	loop
