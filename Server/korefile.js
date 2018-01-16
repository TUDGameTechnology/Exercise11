let project = new Project('Exercise11-Server', __dirname);

project.addFile('../Sources/**');
project.setDebugDir('../Deployment');
project.cpp11 = true;

project.addDefine('MASTER');

Project.createProject('../Kore', __dirname).then((kore) => {
	project.addSubProject(kore);
	resolve(project);
});
