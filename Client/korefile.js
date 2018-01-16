let project = new Project('Exercise11-Client', __dirname);

project.addFile('../Sources/**');
project.setDebugDir('../Deployment');
project.cpp11 = true;

Project.createProject('../Kore', __dirname).then((kore) => {
	project.addSubProject(kore);
	resolve(project);
});
