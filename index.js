const core = require('@actions/core');
const github = require('@actions/github')
const axios = require('axios')

// `who-to-greet` input defined in action metadata file
const bearerToken = core.getInput('bearer-token')

const AuthStr = 'Bearer '.concat(bearerToken)
const headers = { headers: { Authorization: AuthStr, Accept: 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'} }
const url = 'https://api.github.com/repos/Infomaniak/desktop-kDrive/pulls'

async function fetchData(url, headers) {
    try {
        const response = await axios.get(url, headers)
        const data = await response.data
        return data
    } catch (error) {
        console.error('Error fetching data:', error)
    }
}

function extractAutoMergePRs(data) {
	const prNumbers = []
	for(let i = 0; i < data.length; i++) {
		let obj = data[i]
		if (obj.auto_merge) {
	  		prNumbers.push(obj.number)
		}
	}
  return prNumbers
}

async function findMergeablePRs(autoMergePRs) {
	const mergeablePRs = []
	for (let i = 0; i < autoMergePRs.length; i++) {
  		const data = await fetchData(url.concat('/').concat(autoMergePRs[i]), headers)
  		//console.log(data);
  		if (data.mergeable && data.mergeable_state=='behind') {
			mergeablePRs.push(data.number)
  		}
  	}
  	return mergeablePRs;
}

async function updatePRs(mergeablePRs) {
    for (let i = 0; i < mergeablePRs.length; i++) {
        try {
            console.log('Updating PR', mergeablePRs[i])
            const response = await axios.put(url.concat('/').concat(mergeablePRs[i]).concat('/update-branch'), {}, headers)
            console.log('PR', mergeablePRs[i], 'updated!')
        } catch (error) {
            console.error('Error updating branch:', error)
        }
    }
}

async function run() {
  const data = await fetchData(url, headers)
  if(data.length === 0) return;
  //console.log(data);

  const autoMergePRs = extractAutoMergePRs(data)
  if(autoMergePRs.length === 0) return;
  console.log('PRs with auto merge activated:', autoMergePRs)

  const mergeablePRs = await findMergeablePRs(autoMergePRs)
  if(mergeablePRs.length === 0) return;
  console.log('Mergeable PRs:', mergeablePRs)

  await updatePRs(mergeablePRs)
}

run();