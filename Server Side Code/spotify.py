import requests
import operator 

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
	token = 'BQDGCoZGDwukoStaGbRUYf_5AHsEeDrixGWjcmk3dkU-Gus57oJOLKjF6mD9iGbkyXgM7h9TUsKG2PN5gP80SCxb-KE7aLQWAvmEi6zsRdAiIaY16mY08hqKWbRpEIoX8J6WjIU7GpE'
	headers = {'Authorization': 'Bearer '+token}
	url = "https://api.spotify.com/v1/audio-features/{}".format(track_id)
	r = requests.get( url,headers = headers)
	return r.json()
	
def get_danciblity(track_id):
	"""extracts danceability metric from track-specific json
	"""
	return get_track_features(track_id)["danceability"]

def sort_by_danceability(tracks):
	"""sort by danceability
		not all that useful but a good starting point for more sophisticated algorithm
	""" 
	dance_mapping = {}
	for track in tracks:
		track_id = get_track_id(track)
		dance_mapping[track]=get_danciblity(track_id)
	sorted_map = sorted(dance_mapping.items(),key=operator.itemgetter(1)) #list where each element is a tuple of the type (song_name,danceability)
	return sorted_map


tracks = ['humble','DNA','the bird','good dope','summer friends','friends','scrape','loft music','call casting','element','retrograde','rose quartz','good life','with me','get away','AA','no flockin','ATLiens']

