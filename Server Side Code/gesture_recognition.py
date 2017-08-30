import records 
import math 

GESTURE_LENGTH = 100
GESTURE_NAMES = ['skip','rewind','shuffle']
THRESHOLD_FOR_MATCH = 0.1

def lcm(x, y):
   if x > y:            # Store smallest number
       temp = x
   else:
       temp = y
   while (True):    # loop
        # run through temp values until you find one that can
        # mod both x and y with no remainder
       if((temp % x == 0) and (temp % y == 0)):
           lcm = temp
           break
       temp += 1
   return lcm

def offset_and_normalize(inp):
    tot = 0 
    for x in inp:
        tot+=x
    inp_ave = float(tot)/len(inp)
    for i in range(len(inp)):
        inp[i]-=inp_ave
    tot2 = 0
    for x in inp:
        tot2+=x**2
    tot2=math.sqrt(tot2)
    for i in range(len(inp)):
        inp[i]=inp[i]/tot2
    return inp
def cross_correlation(x,y):
    x = offset_and_normalize(x)
    y = offset_and_normalize(y)
    result = 0
    for i in range(len(x)):
        result+=x[i]*y[i]
    return result

def downsample(x,q):
	if q<1 or not(type(q)==int):
		return []
	else:
		return x[::q]
def upsample(x,p):
	result = []
	if not(type(p) == int) or p<1:
		return result
	else:
		for i in range(len(x)-1):
			result.append(x[i])
			dy = float((x[i+1]-x[i]))/p
			for j in range(1,p):
				result.append(x[i]+j*dy)
		result.append(x[len(x)-1])
		for j in range(1,p):
			result.append(x[len(x)-1]+j*dy)
		return result

def resample(inp,desired_length):
	original_length = len(inp)
	result = upsample(inp,desired_length)
	return downsample(result,original_length)


def web_handler(request):
    method = request['REQUEST_METHOD']
    db = records.Database('mysql://student:6s08student@localhost/iesc')
    if method == "GET":
        parameters = request['GET']       
        if 'gesture' in parameters and 'kerberos' in parameters:
            input_data_string = parameters['gesture']
            input_data = [float(x) for x in input_data_string.split(',')]
            resampled_data = resample(input_data,GESTURE_LENGTH)
            cc = {}
            output = ""
            for gname in GESTURE_NAMES:
                query = "SELECT gesture, name FROM team36_gesture WHERE kerberos=:kerberos AND name=:name ORDER BY ID ASC LIMIT 1"
                rows = db.query(query,kerberos=parameters['kerberos'],name=gname)
                dict_rows = rows.as_dict()

                if len(dict_rows) > 0:
                    row = dict_rows[0]
                    # return row['gesture'][1:-1].split(',')
                    gesture_data = [float(x) for x in row['gesture'][1:-1].split(',')]
                    gesture_name = row['name']
                    cc[gesture_name] = cross_correlation(gesture_data,resampled_data)
            if  len(cc) > 0 and max(cc.values()) >= THRESHOLD_FOR_MATCH:
                for name in cc:
                    if cc[name] == max(cc.values()):
                        output =  name
                        break

            else:
                output += "no match"
            return output


        return "Did not get"


    elif method == "POST":
        parameters = request['POST']
        if 'gesture' in parameters and 'kerberos' in parameters and 'gesture_name' in parameters:
            input_data_string = parameters['gesture']
            kerberos = parameters['kerberos']
            gesture_name = parameters['gesture_name']

            input_data = [float(x) for x in input_data_string.split(',')]
            resampled_data = resample(input_data,GESTURE_LENGTH)

            db.query("INSERT into team36_gesture (kerberos, gesture, name) VALUES (:kerb, :data, :name)",
                kerb=kerberos,data=str(resampled_data),name=gesture_name)
            return "Inserted into database!"

        return "Did not insert"

