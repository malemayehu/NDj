for j in range(50):	
	for i in range(50):
		line = ''
		if (i+j)%2==0:
			line+='*'
		else:
			line+=('-')
	print(line)