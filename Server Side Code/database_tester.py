import records
import requests
import operator
db = records.Database('mysql://student:6s08student@localhost/iesc')
def web_handler(request):
	method = request['REQUEST_METHOD']
	if method == 'GET':
		return db.query("SELECT * FROM song_info").as_dict()
	elif method == "POST":
		parameters = request['POST']
		if 'Song_names' in parameters:
			song_names = parameters['Song_names']
			song_names = song_names.split('~')
			for song in song_names:
				id_num = get_track_id(song)
				features = get_track_features(id_num)
				db.query("INSERT INTO song_info (name, track_id, danceability, tempo,energy) VALUES (:name, :id,:dance,:tempo,:energy)",name=song,id=id_num,dance=features['danceability'],tempo=features['tempo'],energy=features['energy'])
			return "Inserted into database!"

def get_track_id(track_name):
	""" gets track id given a track name 
		input: track name as a string 
		output: track id (some number)
	"""
	r = requests.get('https://api.spotify.com/v1/search?q={}&type=track'.format(track_name))
	track_id = r.json()['tracks']['items'][0]['id']
	return track_id

def get_track_features(track_id):
	"""get track features json 
		input: track id
		output: track-specific json 
	"""
	token = 'BQCcCDfLG3ySxlL3ePmpkAYwa0ynnav-4utsBpEPebSY8TJFcw53P-FbmKApNSgLeq2SkmlD0dqpPZ2-PmHiepZMouIWGR-gVJ_egAguE47J6PFoVQbz3fbGh8j7A1VlnTIW0toOkQ8'
	headers = {'Authorization': 'Bearer '+token}
	url = "https://api.spotify.com/v1/audio-features/{}".format(track_id)
	r = requests.get( url,headers = headers)
	return r.json()

def get_danceability(track_id):
	"""extracts danceability metric from track-specific json
	"""
	return get_track_features(track_id)["danceability"]

def get_tempo(track_id):
	return get_track_features(track_id)["tempo"]
def get_energy(track_id):
	return get_track_features(track_id)["energy"]

def sort_by_danceability(tracks):
	"""sort by danceability
		not all that useful but a good starting point for more sophisticated algorithm
	""" 
	dance_mapping = {}
	query = "SELECT name,danceability FROM song_info WHERE name in (" + ','.join(("'"+track+"'" for track in tracks))+")"
	rows = db.query(query)
	dict_rows = rows.as_dict()#Need to fix by creating dictionary 
	new_dict = [dict(t) for t in set([tuple(d.items()) for d in dict_rows])]
	return new_dict
	sorted_map = sorted(dance_mapping.items(),key=operator.itemgetter(1)) #list where each element is a tuple of the type (song_name,danceability)
	result = []
	for ele in sorted_map:
		result.append(ele[0]) #list of song names increasing by danceability
	return result