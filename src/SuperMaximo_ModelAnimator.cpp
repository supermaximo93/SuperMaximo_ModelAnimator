//============================================================================
// Name        : SuperMaximo_ModelAnimator.cpp
// Author      : Max Foster
// Version     : 0.8
// Copyright   : http://creativecommons.org/licenses/by/3.0/
// Description : The SuperMaximo ModelAnimator
//============================================================================

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
using namespace std;

#define GL3_PROTOTYPES 1
#include <GL3/gl3.h>
#include <GL/glu.h>

#include <gtk-3.0/gtk/gtk.h>

#include "SuperMaximo_GameLibrary/headers/SMSDL.h"
#include "SuperMaximo_GameLibrary/headers/Display.h"
#include "SuperMaximo_GameLibrary/headers/Input.h"
#include "SuperMaximo_GameLibrary/headers/Utils.h"
#include "SuperMaximo_GameLibrary/headers/classes/Shader.h"
#include "SuperMaximo_GameLibrary/headers/classes/Model.h"
using namespace SuperMaximo;

enum viewOrientationEnum {
	TOP = 0,
	BOTTOM,
	LEFT,
	RIGHT,
	FRONT,
	BACK,
	FREE,
	VIEW_ORIENTATION_ENUM_COUNT
};

enum axisEnum {
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS
};

enum modeEnum {
	SKELETON_MODE = 0,
	ANIMATION_MODE
};

struct boneIteratorAssociation {
	bone * pBone;
	GtkTreeIter iterator;
};

struct animationDetail {
	string name;
	unsigned length;
};

#define INITIAL_MODEL_Z -10.0f
#define MOUSE_MIDDLE_BORDER 15
#define SHORTCUT_PRESS_DELAY 10.0f
#define DEFAULT_ZOOM 8.0f

#define CONTROL_KEYCODE 306
#define SHIFT_KEYCODE 304
#define ALT_KEYCODE 308
#define ZERO_KEYCODE 48
#define NUMPAD_ZERO_KEYCODE 256
#define UP_KEYCODE 273
#define DOWN_KEYCODE 274
#define RIGHT_KEYCODE 275
#define LEFT_KEYCODE 276
#define SPACEBAR_KEYCODE 32

#define UPPER_LIMIT 0
#define LOWER_LIMIT 1

bool executeOpenFile = false, wireframeModeEnabled = false, boneCreationEnabled = false, skinningEnabled = false, creatingBone = false, trueBool = true,
		falseBool = false, playAnimation = false, autoKeyEnabled = false;
Model * loadedModel = NULL, * boneModel = NULL;
Shader * skeletonShader, * animationShader, * boneShader, * arrowShader, * boxShader, * ringShader;
vec2 lastMousePosition = {{0.0f}, {0.0f}}, viewTranslation = {{0.0f}, {0.0f}};
GtkWidget * wireframeToggleButton, * boneCreationToggleButton, * skinningToggleButton, * viewToggleButton[VIEW_ORIENTATION_ENUM_COUNT], * boneView, * boneScaleSpinButton,
	* animationLengthSpinButton, * timelineJumpEntry, * timeline, * boneWindow, * animationWindow, * switchModeButton, * boneRotationLimitSpinButton[3][2],
	* playAnimationToggleButton, * autoKeyToggleButton, * boneNameEntry, * animationNameEntry, * animationSelectSpinButton;
gulong boneCreationToggleHandler, skinningToggleHandler, viewToggleHandler[VIEW_ORIENTATION_ENUM_COUNT], boneRotationLimitSpinHandler[3][2], playAnimationToggleHandler,
	autoKeyToggleHandler;
float timeSinceShortcutPressed = SHORTCUT_PRESS_DELAY, xRotation = 0.0f, yRotation = 0.0f, zoom = DEFAULT_ZOOM, boneScale = 1.0f;
bone * root = NULL, * selectedBone = NULL;
vector<bone *> boneList;
viewOrientationEnum viewOrientation, viewOrientationArr[VIEW_ORIENTATION_ENUM_COUNT] = {TOP, BOTTOM, LEFT, RIGHT, FRONT, BACK, FREE};
GtkTreeStore * boneStore;
vector<boneIteratorAssociation> boneIteratorAssociations;
GtkTreeSelection * boneSelect;
GLuint arrowVao, arrowVbo, boxVao, boxVbo, ringVao, ringVbo, * modelVbo;
unsigned currentFrame = 1, currentAnimation = 0;
modeEnum mode = SKELETON_MODE;
vector<int> freeBoneIds;
vector<animationDetail> animations;

void createGlWindow();

void destroyGlWindow();

gboolean glLoop(void *);

void deleteBone(bone *);

GtkWidget * createToolsWindow();

GtkWidget * createBoneWindow();

GtkWidget * createAnimationWindow();

void setCurrentFrameFromScale();

void setRotationLimitValues(bone *);

void verifyBoneAnimationCounts(bone * = NULL);

void updateAnimationSpinButtonRange();

void resetBones() {
	if (root != NULL) deleteBone(root);
	boneList.clear();
	boneIteratorAssociations.clear();
	gtk_tree_store_clear(boneStore);
	gtk_entry_set_text(GTK_ENTRY(boneNameEntry), "");
	selectedBone = root = NULL;
}

void resetAnimations() {
	currentFrame = 1;
	currentAnimation = 0;
	animations.clear();
	animations.push_back((animationDetail){"animation0", 60});
	for (unsigned i = 0; i < boneList.size(); i++) {
		boneList[i]->animations.clear();
	}
	if (boneList.size() > 0) verifyBoneAnimationCounts();
}

void resetAll() {
	resetBones();
	resetAnimations();
	if (loadedModel != NULL) {
		delete loadedModel;
		loadedModel = NULL;
	}
}

string getFileNameOpen() {
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL), * dialog = gtk_file_chooser_dialog_new ("Open File", GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	GtkFileFilter * filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, "*.obj");
	gtk_file_filter_add_pattern(filter, "*.smo");
	gtk_file_filter_add_pattern(filter, "*.smm");
	gtk_file_filter_add_pattern(filter, "*.sms");
	gtk_file_filter_add_pattern(filter, "*.sma");
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		string fileName;
		fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_widget_destroy(dialog);
		gtk_widget_destroy(window);
		return fileName;
	}
	gtk_widget_destroy(dialog);
	gtk_widget_destroy(window);
	return "";
}

string getFileNameSave(string caption = "") {
	if (caption == "") caption = "Save File";

	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL), * dialog = gtk_file_chooser_dialog_new (caption.c_str(), GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		string fileName;
		fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gtk_widget_destroy(dialog);
		gtk_widget_destroy(window);
		return fileName;
	}
	gtk_widget_destroy(dialog);
	gtk_widget_destroy(window);
	return "";
}

void exportSms(string fileName = "") {
	if (root == NULL) return;

	//just to make sure that the bones are *definitely* in the correct order!
	bone * boneArray[boneList.size()];
	for (unsigned i = 0; i < boneList.size(); i++) boneArray[boneList[i]->id] = boneList[i];

	if (fileName == "") fileName = getFileNameSave("Saving SuperMaximo Skeleton");

	if (lowerCase(rightStr(fileName, 4)) != ".sms") fileName += ".sms";
	ofstream file;
	file.open(fileName.c_str());

	file << boneList.size() << "\n";
	for (unsigned i = 0; i < boneList.size(); i++) {
		file << boneArray[i]->id << "\n";
		file << boneArray[i]->name << "\n";
		file << boneArray[i]->x << "\n";
		file << boneArray[i]->y << "\n";
		file << boneArray[i]->z << "\n";
		file << boneArray[i]->endX << "\n";
		file << boneArray[i]->endY << "\n";
		file << boneArray[i]->endZ << "\n";
		if (boneArray[i]->parent == NULL) file << "-1\n"; else file << boneArray[i]->parent->id << "\n";
		file << boneArray[i]->rotationUpperLimit.x << "\n";
		file << boneArray[i]->rotationUpperLimit.y << "\n";
		file << boneArray[i]->rotationUpperLimit.z << "\n";
		file << boneArray[i]->rotationLowerLimit.x << "\n";
		file << boneArray[i]->rotationLowerLimit.y << "\n";
		file << boneArray[i]->rotationLowerLimit.z << "\n";
	}
	file.close();
}

void exportSma(string fileName = "") {
	if (root == NULL) return;

	if (fileName == "") fileName = getFileNameSave("Saving SuperMaximo Animation ("+animations[currentAnimation].name+")");

	if (lowerCase(rightStr(fileName, 4)) != ".sma") fileName += ".sma";
	ofstream file;
	file.open(fileName.c_str());

	file << boneList.size() << "\n";
	for (unsigned i = 0; i < boneList.size(); i++) {
		file << boneList[i]->id << "\n";
		file << boneList[i]->animations[currentAnimation].name << "\n";
		file << animations[currentAnimation].length << "\n";
		file << boneList[i]->animations[currentAnimation].frames.size() << "\n";
		for (unsigned j = 0; j < boneList[i]->animations[currentAnimation].frames.size(); j++) {
			file << boneList[i]->animations[currentAnimation].frames[j].xRot << "\n";
			file << boneList[i]->animations[currentAnimation].frames[j].yRot << "\n";
			file << boneList[i]->animations[currentAnimation].frames[j].zRot << "\n";
			file << boneList[i]->animations[currentAnimation].frames[j].step << "\n";
		}
	}
	file.close();
}

void exportSmm(string fileName = "") {
	if (loadedModel == NULL) return;

	if (fileName == "") fileName = getFileNameSave("Saving SuperMaximo Model");

	if (lowerCase(rightStr(fileName, 4)) != ".smm") fileName += ".smm";
	ofstream file;
	file.open(fileName.c_str());

	file << loadedModel->triangles()->size() << "\n";

	unsigned arraySize = loadedModel->triangles()->size()*3*24;
	GLfloat data[arraySize];
	glBindBuffer(GL_ARRAY_BUFFER, *modelVbo);
	glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), &data);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	for (unsigned i = 0; i < arraySize; i++) file << data[i] << "\n";

	file << loadedModel->materials()->size() << "\n";
	for (unsigned i = 0; i < loadedModel->materials()->size(); i++) {
		file << loadedModel->materials()->at(i).fileName << "\n";
	}

	file.close();
}

void exportSmo() {
	string fileName = getFileNameSave("Saving SuperMaximo Object");
	if (lowerCase(rightStr(fileName, 4)) == ".smo") leftStr(&fileName, fileName.size()-4);

	exportSms(fileName);
	exportSmm(fileName);

	vector<string> animationFileNames;
	for (unsigned i = 0; i < animations.size(); i++) {
		string fileName = getFileNameSave("Saving SuperMaximo Animation ("+animations[i].name+")");
		if (lowerCase(rightStr(fileName, 4)) != ".sma") fileName += ".sma";
		exportSma(fileName);

		int pos = fileName.find_last_of("/")+1;
		rightStr(&fileName, fileName.size()-pos);
		animationFileNames.push_back(fileName);
	}

	fileName += ".smo";
	ofstream file;
	file.open(fileName.c_str());

	leftStr(&fileName, fileName.size()-4);
	int pos = fileName.find_last_of("/")+1;
	rightStr(&fileName, fileName.size()-pos);

	file << fileName << ".smm\n";
	file << fileName << ".sms\n";
	for (unsigned i = 0; i < animationFileNames.size(); i++) file << animationFileNames[i] << "\n";
	file.close();
}

void exportSmsCallback() {
	exportSms();
}

void exportSmaCallback() {
	exportSma();
}

void exportSmmCallback() {
	exportSmm();
}

void loadSms(string fileName) {
	vector<string> text;
	ifstream file;
	file.open(fileName.c_str());
	if (file.is_open()) {
		while (!file.eof()) {
			string tempStr;
			getline(file, tempStr);
			if (leftStr(tempStr, 2) != "//") {
				if (rightStr(tempStr, 1) == "\n") leftStr(&tempStr, tempStr.size()-1);
				text.push_back(tempStr);
			}
		}
		file.close();
	} else {
		cout << "File " << fileName << " could not be loaded" << endl;
		return;
	}
	if (text.back() == "") text.pop_back();

	unsigned boneCount = atoi(text.front().c_str()), line = 1;

	for (unsigned i = 0; i < boneCount; i++) {
		bone * newBone = new bone;

		newBone->id = atoi(text[line].c_str());
		line++;
		newBone->name = text[line];
		line++;
		newBone->x = strtof(text[line].c_str(), NULL);
		line++;
		newBone->y = strtof(text[line].c_str(), NULL);
		line++;
		newBone->z = strtof(text[line].c_str(), NULL);
		line++;
		newBone->endX = strtof(text[line].c_str(), NULL);
		line++;
		newBone->endY = strtof(text[line].c_str(), NULL);
		line++;
		newBone->endZ = strtof(text[line].c_str(), NULL);
		line++;
		int boneParentId = atoi(text[line].c_str());
		if (boneParentId < 0) newBone->parent = NULL; else {
			newBone->parent = boneList[boneParentId];
			boneList[boneParentId]->child.push_back(newBone);
		}
		line++;
		newBone->rotationUpperLimit.x = strtof(text[line].c_str(), NULL);
		line++;
		newBone->rotationUpperLimit.y = strtof(text[line].c_str(), NULL);
		line++;
		newBone->rotationUpperLimit.z = strtof(text[line].c_str(), NULL);
		line++;
		newBone->rotationLowerLimit.x = strtof(text[line].c_str(), NULL);
		line++;
		newBone->rotationLowerLimit.y = strtof(text[line].c_str(), NULL);
		line++;
		newBone->rotationLowerLimit.z = strtof(text[line].c_str(), NULL);
		line++;

		boneList.push_back(newBone);
	}
	for (unsigned i = 0; i < boneList.size(); i++) {
		if ((root == NULL) || (selectedBone == NULL)) {
			root = boneList[i];
			selectedBone = root;

			boneIteratorAssociations.push_back((boneIteratorAssociation){selectedBone});
			gtk_tree_store_append(boneStore, &(boneIteratorAssociations.back().iterator), NULL);
		} else {
			selectedBone = boneList[i];

			unsigned i;
			for (i = 0; i < boneIteratorAssociations.size(); i++) {
				if (boneIteratorAssociations[i].pBone == selectedBone->parent) break;
			}

			boneIteratorAssociations.push_back((boneIteratorAssociation){selectedBone});
			gtk_tree_store_append(boneStore, &(boneIteratorAssociations.back().iterator), &(boneIteratorAssociations[i].iterator));
		}
		selectedBone->xRot = selectedBone->yRot = selectedBone->zRot = 0.0f;

		gtk_tree_store_set(boneStore, &(boneIteratorAssociations.back().iterator), 0, selectedBone->name.c_str(), -1);

		GtkTreePath * tempPath = gtk_tree_model_get_path(GTK_TREE_MODEL(boneStore), &(boneIteratorAssociations.back().iterator));
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(boneView), tempPath);
		gtk_tree_path_free(tempPath);
		gtk_tree_selection_select_iter(boneSelect, &(boneIteratorAssociations.back().iterator));
	}
}

void loadSma(string fileName) {
	if (root == NULL) return;

	vector<string> text;
	ifstream file;
	file.open(fileName.c_str());
	if (file.is_open()) {
		while (!file.eof()) {
			string tempStr;
			getline(file, tempStr);
			if (leftStr(tempStr, 2) != "//") {
				if (rightStr(tempStr, 1) == "\n") leftStr(&tempStr, tempStr.size()-1);
				text.push_back(tempStr);
			}
		}
		file.close();
	} else {
		cout << "File " << fileName << " could not be loaded" << endl;
		return;
	}
	if (text.back() == "") text.pop_back();

	unsigned boneCount = atoi(text.front().c_str()), line = 1;
	for (unsigned i = 0; i < boneCount; i++) {
		bone::animation newAnimation;
		unsigned boneId = atoi(text[line].c_str());
		line++;

		newAnimation.name = text[line];
		line++;
		newAnimation.length = atoi(text[line].c_str());
		line++;

		unsigned frameCount = atoi(text[line].c_str());
		line++;
		for (unsigned j = 0; j < frameCount; j++) {
			bone::keyFrame newFrame;
			newFrame.xRot = strtof(text[line].c_str(), NULL);
			line++;
			newFrame.yRot = strtof(text[line].c_str(), NULL);
			line++;
			newFrame.zRot = strtof(text[line].c_str(), NULL);
			line++;
			newFrame.step = atoi(text[line].c_str());
			line++;
			newAnimation.frames.push_back(newFrame);
		}
		if (boneId < boneList.size()) boneList[boneId]->animations.push_back(newAnimation);
	}
	animations.push_back((animationDetail){root->animations.back().name, root->animations.back().length});
	updateAnimationSpinButtonRange();
}

void bufferObj(GLuint * vbo, Model * model, void *) {
	modelVbo = vbo;

	GLfloat vertexArray[model->triangles()->size()*3*24];
	unsigned count = 0;
	for (unsigned i = 0; i < model->triangles()->size(); i++) {
		for (short j = 0; j < 3; j++) {
			vertexArray[count] = (*(model->triangles()))[i].coords[j].x;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].coords[j].y;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].coords[j].z;
			count++;
			vertexArray[count] = 1.0f;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].surfaceNormal().x;//coords[j].normal_.x;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].surfaceNormal().y;//(*(model->triangles()))[i].coords[j].normal_.y;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].surfaceNormal().z;//(*(model->triangles()))[i].coords[j].normal_.z;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].ambientColor.r;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].ambientColor.g;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].ambientColor.b;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].diffuseColor.r;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].diffuseColor.g;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].diffuseColor.b;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].specularColor.r;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].specularColor.g;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].specularColor.b;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].texCoords[j].x;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].texCoords[j].y;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].texCoords[j].z;
			count++;
			vertexArray[count] = (*(model->triangles()))[i].mtlNum;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].hasTexture;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].shininess;
			count++;
			vertexArray[count] = (*(model->materials()))[(*(model->triangles()))[i].mtlNum].alpha;
			count++;
			vertexArray[count] = -1.0f; //bone ID
			count++;
		}
	}

	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexArray), vertexArray, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(VERTEX_ATTRIBUTE, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, 0);
	glVertexAttribPointer(NORMAL_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*4));
	glVertexAttribPointer(COLOR0_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*7));
	glVertexAttribPointer(COLOR1_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*10));
	glVertexAttribPointer(COLOR2_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*13));
	glVertexAttribPointer(TEXTURE0_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*16));
	glVertexAttribPointer(EXTRA0_ATTRIBUTE, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*19));
	glVertexAttribPointer(EXTRA1_ATTRIBUTE, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*20));
	glVertexAttribPointer(EXTRA2_ATTRIBUTE, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*21));
	glVertexAttribPointer(EXTRA3_ATTRIBUTE, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*22));
	glVertexAttribPointer(EXTRA4_ATTRIBUTE, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*24, (const GLvoid*)(sizeof(GLfloat)*23));

	glEnableVertexAttribArray(VERTEX_ATTRIBUTE);
	glEnableVertexAttribArray(NORMAL_ATTRIBUTE);
	glEnableVertexAttribArray(COLOR0_ATTRIBUTE);
	glEnableVertexAttribArray(COLOR1_ATTRIBUTE);
	glEnableVertexAttribArray(COLOR2_ATTRIBUTE);
	glEnableVertexAttribArray(TEXTURE0_ATTRIBUTE);
	glEnableVertexAttribArray(EXTRA0_ATTRIBUTE);
	glEnableVertexAttribArray(EXTRA1_ATTRIBUTE);
	glEnableVertexAttribArray(EXTRA2_ATTRIBUTE);
	glEnableVertexAttribArray(EXTRA3_ATTRIBUTE);
	glEnableVertexAttribArray(EXTRA4_ATTRIBUTE);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void flagExecuteOpenFile() {
	executeOpenFile = !executeOpenFile;
}

void loadBonesFromModel() {
	for (unsigned i = 0; i < loadedModel->bones()->size(); i++) {
		if ((root == NULL) || (selectedBone == NULL)) {
			root = loadedModel->bones()->at(i);
			selectedBone = root;

			boneIteratorAssociations.push_back((boneIteratorAssociation){selectedBone});
			gtk_tree_store_append(boneStore, &(boneIteratorAssociations.back().iterator), NULL);
		} else {
			selectedBone = loadedModel->bones()->at(i);

			unsigned i;
			for (i = 0; i < boneIteratorAssociations.size(); i++) {
				if (boneIteratorAssociations[i].pBone == selectedBone->parent) break;
			}

			boneIteratorAssociations.push_back((boneIteratorAssociation){selectedBone});
			gtk_tree_store_append(boneStore, &(boneIteratorAssociations.back().iterator), &(boneIteratorAssociations[i].iterator));
		}
		selectedBone->xRot = selectedBone->yRot = selectedBone->zRot = 0.0f;

		gtk_tree_store_set(boneStore, &(boneIteratorAssociations.back().iterator), 0, selectedBone->name.c_str(), -1);

		GtkTreePath * tempPath = gtk_tree_model_get_path(GTK_TREE_MODEL(boneStore), &(boneIteratorAssociations.back().iterator));
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW(boneView), tempPath);
		gtk_tree_path_free(tempPath);
		gtk_tree_selection_select_iter(boneSelect, &(boneIteratorAssociations.back().iterator));
		boneList.push_back(selectedBone);
	}
	loadedModel->bones()->clear();
	if (selectedBone != NULL) {
		setRotationLimitValues(selectedBone);
		if (animations.size() > 0) animations.clear();
		for (unsigned i = 0; i < selectedBone->animations.size(); i++)
			animations.push_back((animationDetail){selectedBone->animations[i].name, selectedBone->animations[i].length});
	}
}

void openFile() {
	string fileName = getFileNameOpen();
	if (fileName != "") {
		string tempStr = rightStr(fileName, 4);
		if ((tempStr == ".obj") || (tempStr == ".smo") || (tempStr == ".smm") || (tempStr == ".sms") || (tempStr == ".sma")) {
			int pos = fileName.find_last_of("/")+1;
			switch (tempStr[3]) {
			case 'j':
				if (loadedModel != NULL) delete loadedModel;
				loadedModel = new Model("model", leftStr(fileName, pos), rightStr(fileName, fileName.size()-pos), 60, DYNAMIC_DRAW, bufferObj);
				break;
			case 'o':
				if (loadedModel != NULL) delete loadedModel;
				loadedModel = new Model("model", leftStr(fileName, pos), rightStr(fileName, fileName.size()-pos), 60, DYNAMIC_DRAW);
				modelVbo = loadedModel->vboPointer();
				loadBonesFromModel();
				break;
			case 'm':
				if (loadedModel != NULL) delete loadedModel;
				loadedModel = new Model("model", leftStr(fileName, pos), rightStr(fileName, fileName.size()-pos), 60, DYNAMIC_DRAW);
				modelVbo = loadedModel->vboPointer();
				break;
			case 's':
				resetBones();
				loadSms(fileName);
				break;
			case 'a':
				resetAnimations();
				loadSma(fileName);
				break;
			}
		} else cout << "Invalid file type" << endl;
	}
}

void toggleWireframeMode() {
	wireframeModeEnabled = !wireframeModeEnabled;
}

void toggleBoneCreation() {
	boneCreationEnabled = !boneCreationEnabled;
	if (viewOrientation == FREE) {
		g_signal_handler_block(viewToggleButton[FREE], viewToggleHandler[FREE]);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewToggleButton[FREE]), 0);
		g_signal_handler_unblock(viewToggleButton[FREE], viewToggleHandler[FREE]);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewToggleButton[FRONT]), 1);
	}

	if (skinningEnabled) {
		g_signal_handler_block(skinningToggleButton, skinningToggleHandler);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(skinningToggleButton), 0);
		g_signal_handler_unblock(skinningToggleButton, skinningToggleHandler);
		skinningEnabled = false;
	}
}

void toggleSkinning() {
	skinningEnabled = !skinningEnabled;

	if (boneCreationEnabled) {
		g_signal_handler_block(boneCreationToggleButton, boneCreationToggleHandler);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(boneCreationToggleButton), 0);
		g_signal_handler_unblock(boneCreationToggleButton, boneCreationToggleHandler);
		boneCreationEnabled = false;
	}
}

void togglePlayAnimation() {
	playAnimation = !playAnimation;
}

void toggleAutoKey() {
	autoKeyEnabled = !autoKeyEnabled;
}

void setViewOrientation(GtkWidget *, viewOrientationEnum * newViewOrientation) {
	viewOrientation = *newViewOrientation;
	for (int i = TOP; i <= FREE; i++) {
		if (i != viewOrientation) {
			g_signal_handler_block(viewToggleButton[i], viewToggleHandler[i]);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewToggleButton[i]), 0);
			g_signal_handler_unblock(viewToggleButton[i], viewToggleHandler[i]);
		} else {
			//So you can't untoggle a view orientation button by clicking on it
			g_signal_handler_block(viewToggleButton[i], viewToggleHandler[i]);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewToggleButton[i]), 1);
			g_signal_handler_unblock(viewToggleButton[i], viewToggleHandler[i]);
		}
	}
	switch (viewOrientation) {
	case TOP:
		xRotation = 90.0f;
		yRotation = 0.0f;
		break;
	case BOTTOM:
		xRotation = -90.0f;
		yRotation = 0.0f;
		break;
	case LEFT:
		xRotation = 0.0f;
		yRotation = 90.0f;
		break;
	case RIGHT:
		xRotation = 0.0f;
		yRotation = -90.0f;
		break;
	case FRONT:
		xRotation = 0.0f;
		yRotation = 0.0f;
		break;
	case BACK:
		xRotation = 0.0f;
		yRotation = 180.0f;
		break;
	case FREE:
		if (boneCreationEnabled) {
			g_signal_handler_block(boneCreationToggleButton, boneCreationToggleHandler);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(boneCreationToggleButton), 0);
			g_signal_handler_unblock(boneCreationToggleButton, boneCreationToggleHandler);
			boneCreationEnabled = false;
		}
		break;
	default: break;
	}
}

void createGlWindow() {
	initSDL(SDL_INIT_EVERYTHING);
	initDisplay(800, 600, 1000, 60, false, "SuperMaximo ModelAnimator");
	setClearColor(0.3, 0.3, 0.3, 1.0);
	initInput();

	boneShader = new Shader("boneShader", "shaders/bone_vertex_shader.vs", "shaders/bone_fragment_shader.fs", 10, VERTEX_ATTRIBUTE, "vertex",
			NORMAL_ATTRIBUTE, "normal", COLOR0_ATTRIBUTE, "ambientColor", COLOR1_ATTRIBUTE, "diffuseColor", COLOR2_ATTRIBUTE, "specularColor", TEXTURE0_ATTRIBUTE,
			"texCoords", EXTRA0_ATTRIBUTE, "mtlNum", EXTRA1_ATTRIBUTE, "hasTexture", EXTRA2_ATTRIBUTE, "shininess", EXTRA3_ATTRIBUTE, "alpha");
	boneShader->setUniformLocation(MODELVIEW_LOCATION, "modelviewMatrix");
	boneShader->setUniformLocation(PROJECTION_LOCATION, "projectionMatrix");
	boneShader->setUniformLocation(TEXSAMPLER_LOCATION, "endVertex");
	boneShader->setUniformLocation(EXTRA0_LOCATION, "boneModelviewMatrix");
	boneShader->setUniformLocation(EXTRA1_LOCATION, "selected");

	skeletonShader = new Shader("skeletonShader", "shaders/skeleton_vertex_shader.vs", "shaders/skeleton_fragment_shader.fs", 10, VERTEX_ATTRIBUTE, "vertex",
			NORMAL_ATTRIBUTE, "normal", COLOR0_ATTRIBUTE, "ambientColor", COLOR1_ATTRIBUTE, "diffuseColor", COLOR2_ATTRIBUTE, "specularColor", TEXTURE0_ATTRIBUTE,
			"texCoords", EXTRA0_ATTRIBUTE, "mtlNum", EXTRA1_ATTRIBUTE, "hasTexture", EXTRA2_ATTRIBUTE, "shininess", EXTRA3_ATTRIBUTE, "alpha", EXTRA4_ATTRIBUTE, "boneId");
	skeletonShader->setUniformLocation(MODELVIEW_LOCATION, "modelviewMatrix");
	skeletonShader->setUniformLocation(PROJECTION_LOCATION, "projectionMatrix");
	skeletonShader->setUniformLocation(TEXSAMPLER_LOCATION, "colorMap");
	skeletonShader->setUniformLocation(EXTRA0_LOCATION, "jointModelviewMatrix");
	skeletonShader->setUniformLocation(EXTRA1_LOCATION, "selectedBoneId");
	skeletonShader->setUniformLocation(EXTRA2_LOCATION, "polygonModePoint");
	skeletonShader->bind();

	animationShader = new Shader("animationShader", "shaders/animation_vertex_shader.vs", "shaders/animation_fragment_shader.fs", 10, VERTEX_ATTRIBUTE, "vertex",
			NORMAL_ATTRIBUTE, "normal", COLOR0_ATTRIBUTE, "ambientColor", COLOR1_ATTRIBUTE, "diffuseColor", COLOR2_ATTRIBUTE, "specularColor", TEXTURE0_ATTRIBUTE,
			"texCoords", EXTRA0_ATTRIBUTE, "mtlNum", EXTRA1_ATTRIBUTE, "hasTexture", EXTRA2_ATTRIBUTE, "shininess", EXTRA3_ATTRIBUTE, "alpha", EXTRA4_ATTRIBUTE, "boneId");
	animationShader->setUniformLocation(MODELVIEW_LOCATION, "modelviewMatrix");
	animationShader->setUniformLocation(PROJECTION_LOCATION, "projectionMatrix");
	animationShader->setUniformLocation(TEXSAMPLER_LOCATION, "colorMap");
	animationShader->setUniformLocation(EXTRA0_LOCATION, "boneModelviewMatrix");

	arrowShader = new Shader("arrowShader", "shaders/arrow_vertex_shader.vs", "shaders/arrow_fragment_shader.fs", 1, VERTEX_ATTRIBUTE, "vertex");
	arrowShader->setUniformLocation(MODELVIEW_LOCATION, "modelviewMatrix");
	arrowShader->setUniformLocation(PROJECTION_LOCATION, "projectionMatrix");
	arrowShader->setUniformLocation(TEXSAMPLER_LOCATION, "color");
	arrowShader->setUniformLocation(EXTRA0_LOCATION, "arrowLength");

	boxShader = new Shader("boxShader", "shaders/box_vertex_shader.vs", "shaders/arrow_fragment_shader.fs", 1, VERTEX_ATTRIBUTE, "vertex");
	boxShader->setUniformLocation(MODELVIEW_LOCATION, "modelviewMatrix");
	boxShader->setUniformLocation(PROJECTION_LOCATION, "projectionMatrix");
	boxShader->setUniformLocation(TEXSAMPLER_LOCATION, "color");
	boxShader->setUniformLocation(EXTRA0_LOCATION, "startPosition");
	boxShader->setUniformLocation(EXTRA1_LOCATION, "mousePosition");

	ringShader = new Shader("ringShader", "shaders/ring_vertex_shader.vs", "shaders/arrow_fragment_shader.fs", 1, VERTEX_ATTRIBUTE, "vertex");
	ringShader->setUniformLocation(MODELVIEW_LOCATION, "modelviewMatrix");
	ringShader->setUniformLocation(PROJECTION_LOCATION, "projectionMatrix");
	ringShader->setUniformLocation(TEXSAMPLER_LOCATION, "color");
	ringShader->setUniformLocation(EXTRA0_LOCATION, "scale");

	{
		glGenVertexArrays(1, &arrowVao);
		glBindVertexArray(arrowVao);
		glGenBuffers(1, &arrowVbo);
		glBindBuffer(GL_ARRAY_BUFFER, arrowVbo);
		GLfloat vertexArray[24] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
								0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
								0.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexArray), vertexArray, GL_STATIC_DRAW);
		glVertexAttribPointer(VERTEX_ATTRIBUTE, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(VERTEX_ATTRIBUTE);
		glBindVertexArray(0);
	}

	{
		glGenVertexArrays(1, &boxVao);
		glBindVertexArray(boxVao);
		glGenBuffers(1, &boxVbo);
		glBindBuffer(GL_ARRAY_BUFFER, boxVbo);
		GLfloat vertexArray[16] = {0.0f, 0.0f, 0.0f, 1.0f,
								0.0f, 1.0f, 0.0f, 1.0f,
								1.0f, 1.0f, 0.0f, 1.0f,
								1.0f, 0.0f, 0.0f, 1.0f};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexArray), vertexArray, GL_STATIC_DRAW);
		glVertexAttribPointer(VERTEX_ATTRIBUTE, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(VERTEX_ATTRIBUTE);
		glBindVertexArray(0);
	}

	{
		glGenVertexArrays(1, &ringVao);
		glBindVertexArray(ringVao);
		glGenBuffers(1, &ringVbo);
		glBindBuffer(GL_ARRAY_BUFFER, ringVbo);
		GLfloat vertexArray[80];
		short count = 0;
		for (short i = 0; i < 20; i++) {
			vertexArray[count] = sin(degToRad(float(18*i)));
			count++;
			vertexArray[count] = cos(degToRad(float(18*i)));
			count++;
			vertexArray[count] = 0.0f;
			count++;
			vertexArray[count] = 1.0f;
			count++;
		}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexArray), vertexArray, GL_STATIC_DRAW);
		glVertexAttribPointer(VERTEX_ATTRIBUTE, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(VERTEX_ATTRIBUTE);
		glBindVertexArray(0);
	}

	boneModel = new Model("boneModel", "", "bone.obj");
	boneModel->bindShader(boneShader);

	translateMatrix(screenWidth()/2.0f, screenHeight()/2.0f, -500.0f);
	scaleMatrix(zoom, -zoom, zoom);

	g_timeout_add(16, glLoop, NULL);
}

void destroyGlWindow() {
	if (loadedModel != NULL) {
		delete loadedModel;
		loadedModel = NULL;
	}

	glDeleteBuffers(1, &arrowVbo);
	glDeleteVertexArrays(1, &arrowVao);

	glDeleteBuffers(1, &boxVbo);
	glDeleteVertexArrays(1, &boxVao);

	glDeleteBuffers(1, &ringVbo);
	glDeleteVertexArrays(1, &ringVao);

	if (root != NULL) deleteBone(root);
	boneList.clear();

	delete ringShader;
	delete boxShader;
	delete arrowShader;
	delete boneShader;
	delete skeletonShader;
	delete boneModel;

	quitInput();
	quitDisplay();
	quitSDL();
}

void drawBone(bone * pBone) {
	if (pBone == NULL) return;
	pushMatrix();
		float x = pBone->endX, y = pBone->endY, z = pBone->endZ;
		float xRot = 0.0f, yRot = 0.0f, zRot = 0.0f;
		xRot = radToDeg(atan(z/y));
		zRot = radToDeg(atan(y/x));
		if (zRot == 0.0f) {
			if (z > 0) zRot = radToDeg(atan(z/x)); else zRot = radToDeg(atan(-z/x));
		}

		translateMatrix(pBone->x, pBone->y, pBone->z);
		rotateMatrix(pBone->xRot, 1.0f, 0.0f, 0.0f);
		rotateMatrix(pBone->yRot, 0.0f, 1.0f, 0.0f);
		rotateMatrix(pBone->zRot, 0.0f, 0.0f, 1.0f);
		translateMatrix(-pBone->x, -pBone->y, -pBone->z);

		boneShader->use();
		boneShader->setUniform4(TEXSAMPLER_LOCATION, pBone->x+pBone->endX, pBone->y+pBone->endY, pBone->z+pBone->endZ, 1.0f);
		boneShader->setUniform16(EXTRA0_LOCATION, getMatrix(MODELVIEW_MATRIX));
		if (pBone == selectedBone) boneShader->setUniform1(EXTRA1_LOCATION, 1); else boneShader->setUniform1(EXTRA1_LOCATION, 0);

		boneModel->draw(pBone->x, pBone->y, pBone->z, xRot, yRot, zRot-90.0f, boneScale, boneScale, boneScale);

		for (unsigned i = 0; i < pBone->child.size(); i++) drawBone(pBone->child[i]);
	popMatrix();
}

unsigned countBones(bone * startBone) {
	if (startBone == NULL) return 0;
	unsigned count = 1;
	for (unsigned i = 0; i < startBone->child.size(); i++) {
		count += countBones(startBone->child[i]);
	}
	return count;
}

void setRotationLimitValues(bone * pBone) {
	for (short i = 0; i < 3; i++) {
		for (short j = 0; j < 2; j++) g_signal_handler_block(boneRotationLimitSpinButton[i][j], boneRotationLimitSpinHandler[i][j]);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT]), pBone->rotationUpperLimit.x);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT]), pBone->rotationUpperLimit.y);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT]), pBone->rotationUpperLimit.z);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT]), pBone->rotationLowerLimit.x);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT]), pBone->rotationLowerLimit.y);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT]), pBone->rotationLowerLimit.z);

	for (short i = 0; i < 3; i++) {
		for (short j = 0; j < 2; j++) g_signal_handler_unblock(boneRotationLimitSpinButton[i][j], boneRotationLimitSpinHandler[i][j]);
	}
}

void setBoneRotations(float frame, bone * pBone = NULL) {
	if (pBone == NULL) pBone = root;

	int index = pBone->animations[currentAnimation].frameIndex(frame);
	if (index == -1) {
		if (frame > pBone->animations[currentAnimation].frames.back().step) {
			pBone->xRot = pBone->animations[currentAnimation].frames.back().xRot;
			pBone->yRot = pBone->animations[currentAnimation].frames.back().yRot;
			pBone->zRot = pBone->animations[currentAnimation].frames.back().zRot;
		} else {
			bone::keyFrame previousFrame, nextFrame;
			int i = 0;
			while (frame > pBone->animations[currentAnimation].frames[i].step) i++;
			previousFrame = pBone->animations[currentAnimation].frames[i-1];
			nextFrame = pBone->animations[currentAnimation].frames[i];

			float xDiff, yDiff, zDiff, xDiff1 = nextFrame.xRot-previousFrame.xRot, yDiff1 = nextFrame.yRot-previousFrame.yRot, zDiff1 = nextFrame.zRot-previousFrame.zRot,
					xDiff2 = (360.0f-abs(previousFrame.xRot))-abs(nextFrame.xRot), yDiff2 = (360.0f-abs(previousFrame.yRot))-abs(nextFrame.yRot),
					zDiff2 = (360.0f-abs(previousFrame.zRot))-abs(nextFrame.zRot), stepDiff = nextFrame.step-previousFrame.step;

			xDiff = (abs(xDiff1) < xDiff2) ? xDiff1 : ((previousFrame.xRot < 0.0f) ? -xDiff2 : xDiff2);
			yDiff = (abs(yDiff1) < yDiff2) ? yDiff1 : ((previousFrame.yRot < 0.0f) ? -yDiff2 : yDiff2);
			zDiff = (abs(zDiff1) < zDiff2) ? zDiff1 : ((previousFrame.zRot < 0.0f) ? -zDiff2 : zDiff2);

			float multiplier = (frame-previousFrame.step)/stepDiff;
			pBone->xRot = previousFrame.xRot+(xDiff*multiplier);
			pBone->yRot = previousFrame.yRot+(yDiff*multiplier);
			pBone->zRot = previousFrame.zRot+(zDiff*multiplier);
		}
	} else {
		pBone->xRot = pBone->animations[currentAnimation].frames[index].xRot;
		pBone->yRot = pBone->animations[currentAnimation].frames[index].yRot;
		pBone->zRot = pBone->animations[currentAnimation].frames[index].zRot;
	}

	for (unsigned i = 0; i < pBone->child.size(); i++) setBoneRotations(frame, pBone->child[i]);
}

void resetBoneRotations(bone * pBone = NULL) {
	if (pBone == NULL) pBone = root;
	pBone->xRot = 0.0f;
	pBone->yRot = 0.0f;
	pBone->zRot = 0.0f;

	for (unsigned i = 0; i < pBone->child.size(); i++) resetBoneRotations(pBone->child[i]);
}

void initBone(bone * pBone, bone * parent = NULL) {
	static int id = 0;
	int idToUse;
	if (freeBoneIds.size() > 0) {
		idToUse = freeBoneIds.front();
		freeBoneIds.erase(freeBoneIds.begin());
	} else {
		idToUse = id;
		id++;
	}

	stringstream stream(stringstream::in | stringstream::out);
	stream.setf(ios::fixed, ios::floatfield);
	stream << idToUse;

	*pBone = (bone){idToUse, "bone"+stream.str(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, parent};
	pBone->rotationUpperLimit.x = pBone->rotationUpperLimit.y = pBone->rotationUpperLimit.z = 180.0f;
	pBone->rotationLowerLimit.x = pBone->rotationLowerLimit.y = pBone->rotationLowerLimit.z = -180.0f;
	boneList.push_back(pBone);
}

void deleteBone(bone * pBone) {
	for (unsigned i = 0; i < pBone->child.size(); i++) deleteBone(pBone->child[i]);

	if (pBone->parent != NULL) {
		for (unsigned i = 0; i < pBone->parent->child.size(); i++) {
			if (pBone->parent->child[i] == pBone) {
				pBone->parent->child.erase(pBone->parent->child.begin()+i);
				break;
			}
		}
	}

	bool boneFound = false, parentFound;
	if (pBone->parent == NULL) parentFound = true; else parentFound = false;
	for (unsigned i = 0; i < boneIteratorAssociations.size(); i++) {
		if ((boneIteratorAssociations[i].pBone == pBone) && !boneFound) {
			gtk_tree_store_remove(boneStore, &(boneIteratorAssociations[i].iterator));
			boneIteratorAssociations.erase(boneIteratorAssociations.begin()+i);
			boneFound = true;
		} else if ((boneIteratorAssociations[i].pBone == pBone->parent) && !parentFound) {
			gtk_tree_selection_select_iter(boneSelect, &(boneIteratorAssociations[i].iterator));
			parentFound = true;
		}
		if (boneFound && parentFound) break;
	}

	for (unsigned i = 0; i < boneList.size(); i++) {
		if (boneList[i] == pBone) {
			boneList.erase(boneList.begin()+i);
			break;
		}
	}

	unsigned i = 0;
	for (; i < freeBoneIds.size(); i++) {
		if (pBone->id < freeBoneIds[i]) break;
	}
	freeBoneIds.insert(freeBoneIds.begin()+i, pBone->id);

	vector<GLfloat> oldIds;
	vector<GLfloat> newIds;
	for (unsigned i = 0; i < boneList.size(); i++) {
		if (boneList[i]->id >= (int)boneList.size()) {
			oldIds.push_back(boneList[i]->id);

			bone * tempBone = boneList[i];
			boneList.erase(boneList.begin()+i);

			tempBone->id = freeBoneIds.front();
			freeBoneIds.erase(freeBoneIds.begin());
			newIds.push_back(tempBone->id);

			boneList.insert(boneList.begin()+tempBone->id, tempBone);
		}
	}

	if (loadedModel != NULL) {
		glBindBuffer(GL_ARRAY_BUFFER, *modelVbo);
		for (unsigned i = 0; i < loadedModel->triangles()->size(); i++) {
			for (short j = 0; j < 3; j++) {
				GLfloat data;
				glGetBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
				if (data == (GLfloat)pBone->id) {
					data = -1;
					glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
				} else {
					for (unsigned k = 0; k < oldIds.size(); k++) {
						if (data == oldIds[k]) {
							data = newIds[k];
							glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
							break;
						}
					}
				}
			}
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (pBone == root) root = NULL;
	delete pBone;
}

void renameBone() {
	if (selectedBone == NULL) return;

	selectedBone->name = gtk_entry_get_text(GTK_ENTRY(boneNameEntry));
	if (selectedBone->name == "") {
		if (selectedBone == root) selectedBone->name = "root"; else {
			stringstream stream(stringstream::in | stringstream::out);
			stream.setf(ios::fixed, ios::floatfield);
			stream << selectedBone->id;
			selectedBone->name = "bone"+stream.str();
		}
		gtk_entry_set_text(GTK_ENTRY(boneNameEntry), selectedBone->name.c_str());
	}

	for (unsigned i = 0; i < boneIteratorAssociations.size(); i++) {
		if (boneIteratorAssociations[i].pBone == selectedBone) {
			gtk_tree_store_set(boneStore, &boneIteratorAssociations[i].iterator, 0, selectedBone->name.c_str(), -1);
			break;
		}
	}
}

void setAnimationMarks(bone * pBone) {
	if ((mode != ANIMATION_MODE) || pBone == NULL) return;
	gtk_scale_clear_marks(GTK_SCALE(timeline));
	for (unsigned i = 0; i < pBone->animations[currentAnimation].frames.size(); i++)
		gtk_scale_add_mark(GTK_SCALE(timeline), pBone->animations[currentAnimation].frames[i].step, GTK_POS_BOTTOM, NULL);
}

void selectBone(GtkTreeSelection * selection) {
	GtkTreeIter iterator;
	if (gtk_tree_selection_get_selected(selection, NULL, &iterator)) {
		gchar * gIteratorPath = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(boneStore), &iterator);
		string iteratorPath = gIteratorPath;
		g_free(gIteratorPath);
		for (unsigned i = 0; i < boneIteratorAssociations.size(); i++) {
			gchar * gTempPath = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(boneStore), &(boneIteratorAssociations[i].iterator));
			string tempPath = gTempPath;
			g_free(gTempPath);
			if (tempPath == iteratorPath) {
				selectedBone = boneIteratorAssociations[i].pBone;
				setRotationLimitValues(selectedBone);
				setAnimationMarks(selectedBone);
				gtk_entry_set_text(GTK_ENTRY(boneNameEntry), selectedBone->name.c_str());
				break;
			}
		}
	}
}

void updateBoneScale() {
	boneScale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneScaleSpinButton));
}

void setBoneScale(float amount) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneScaleSpinButton), amount);
	updateBoneScale();
}

void updateBoneCoords(bone * startBone) {
	if (startBone->parent != NULL) {
		startBone->x = startBone->parent->x+startBone->parent->endX;
		startBone->y = startBone->parent->y+startBone->parent->endY;
		startBone->z = startBone->parent->z+startBone->parent->endZ;
	}
	for (unsigned i = 0; i < startBone->child.size(); i++) updateBoneCoords(startBone->child[i]);
}

void updateRotations(bone * startBone, unsigned frame, bool setBoneRotation = false) {
	if (setBoneRotation) setBoneRotations(frame);

	int frameIndex = startBone->animations[currentAnimation].frameIndex(frame);

	if ((startBone->rotationUpperLimit.x == 180.0f) && (startBone->rotationLowerLimit.x == -180.0f)) {
		if (startBone->xRot > 180.0f) startBone->xRot -= 360.0f; else if (startBone->xRot < -180.0f) startBone->xRot += 360.0f;
	}
	if (startBone->xRot > startBone->rotationUpperLimit.x) {
		if (startBone->parent != NULL) {
			startBone->parent->xRot += startBone->xRot-startBone->rotationUpperLimit.x;

			if (autoKeyEnabled || (frame != currentFrame)) {
				int frameIndex = startBone->parent->animations[currentAnimation].frameIndex(frame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){startBone->parent->xRot, startBone->parent->yRot, startBone->parent->zRot, frame};
					startBone->parent->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					startBone->parent->animations[currentAnimation].frames[frameIndex].xRot = startBone->parent->xRot;
				}
			}
		}
		startBone->xRot = startBone->rotationUpperLimit.x;
		if (autoKeyEnabled || (frame != currentFrame)) startBone->animations[currentAnimation].frames[frameIndex].xRot = startBone->xRot;

	} else if (startBone->xRot < startBone->rotationLowerLimit.x) {
		if (startBone->parent != NULL) {
			startBone->parent->xRot += startBone->xRot-startBone->rotationLowerLimit.x;

			if (autoKeyEnabled || (frame != currentFrame)) {
				int frameIndex = startBone->parent->animations[currentAnimation].frameIndex(frame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){startBone->parent->xRot, startBone->parent->yRot, startBone->parent->zRot, frame};
					startBone->parent->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					startBone->parent->animations[currentAnimation].frames[frameIndex].xRot = startBone->parent->xRot;
				}
			}
		}
		startBone->xRot = startBone->rotationLowerLimit.x;
		if (autoKeyEnabled || (frame != currentFrame)) startBone->animations[currentAnimation].frames[frameIndex].xRot = startBone->xRot;
	}


	if ((startBone->rotationUpperLimit.y == 180.0f) && (startBone->rotationLowerLimit.y == -180.0f)) {
		if (startBone->yRot > 180.0f) startBone->yRot -= 360.0f; else if (startBone->yRot < -180.0f) startBone->yRot += 360.0f;
	}
	if (startBone->yRot > startBone->rotationUpperLimit.y) {
		if (startBone->parent != NULL) {
			startBone->parent->yRot += startBone->yRot-startBone->rotationUpperLimit.y;

			if (autoKeyEnabled || (frame != currentFrame)) {
				int frameIndex = startBone->parent->animations[currentAnimation].frameIndex(frame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){startBone->parent->xRot, startBone->parent->yRot, startBone->parent->zRot, frame};
					startBone->parent->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					startBone->parent->animations[currentAnimation].frames[frameIndex].yRot = startBone->parent->yRot;
				}
			}
		}
		startBone->yRot = startBone->rotationUpperLimit.y;
		if (autoKeyEnabled || (frame != currentFrame)) startBone->animations[currentAnimation].frames[frameIndex].yRot = startBone->yRot;

	} else if (startBone->yRot < startBone->rotationLowerLimit.y) {
		if (startBone->parent != NULL) {
			startBone->parent->yRot += startBone->yRot-startBone->rotationLowerLimit.y;

			if (autoKeyEnabled || (frame != currentFrame)) {
				int frameIndex = startBone->parent->animations[currentAnimation].frameIndex(frame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){startBone->parent->xRot, startBone->parent->yRot, startBone->parent->zRot, frame};
					startBone->parent->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					startBone->parent->animations[currentAnimation].frames[frameIndex].yRot = startBone->parent->yRot;
				}
			}
		}
		startBone->yRot = startBone->rotationLowerLimit.y;
		if (autoKeyEnabled || (frame != currentFrame)) startBone->animations[currentAnimation].frames[frameIndex].yRot = startBone->yRot;
	}


	if ((startBone->rotationUpperLimit.z == 180.0f) && (startBone->rotationLowerLimit.z == -180.0f)) {
		if (startBone->zRot > 180.0f) startBone->zRot -= 360.0f; else if (startBone->zRot < -180.0f) startBone->zRot += 360.0f;
	}
	if (startBone->zRot > startBone->rotationUpperLimit.z) {
		if (startBone->parent != NULL) {
			startBone->parent->zRot += startBone->zRot-startBone->rotationUpperLimit.z;

			if (autoKeyEnabled || (frame != currentFrame)) {
				int frameIndex = startBone->parent->animations[currentAnimation].frameIndex(frame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){startBone->parent->xRot, startBone->parent->yRot, startBone->parent->zRot, frame};
					startBone->parent->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					startBone->parent->animations[currentAnimation].frames[frameIndex].zRot = startBone->parent->zRot;
				}
			}
		}
		startBone->zRot = startBone->rotationUpperLimit.z;
		if (autoKeyEnabled || (frame != currentFrame)) startBone->animations[currentAnimation].frames[frameIndex].zRot = startBone->zRot;

	} else if (startBone->zRot < startBone->rotationLowerLimit.z) {
		if (startBone->parent != NULL) {
			startBone->parent->zRot += startBone->zRot-startBone->rotationLowerLimit.z;

			if (autoKeyEnabled || (frame != currentFrame)) {
				int frameIndex = startBone->parent->animations[currentAnimation].frameIndex(frame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){startBone->parent->xRot, startBone->parent->yRot, startBone->parent->zRot, frame};
					startBone->parent->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					startBone->parent->animations[currentAnimation].frames[frameIndex].zRot = startBone->parent->zRot;
				}
			}
		}
		startBone->zRot = startBone->rotationLowerLimit.z;
		if (autoKeyEnabled || (frame != currentFrame)) startBone->animations[currentAnimation].frames[frameIndex].zRot = startBone->zRot;
	}
	if (startBone->parent != NULL) updateRotations(startBone->parent, frame);

	if (setBoneRotation) setBoneRotations(currentFrame);
}

void updateBoneRotationLimits() {
	if (selectedBone == NULL) return;

	selectedBone->rotationUpperLimit.x = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT]));
	selectedBone->rotationUpperLimit.y = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT]));
	selectedBone->rotationUpperLimit.z = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT]));

	selectedBone->rotationLowerLimit.x = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT]));
	selectedBone->rotationLowerLimit.y = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT]));
	selectedBone->rotationLowerLimit.z = gtk_spin_button_get_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT]));

	for (unsigned i = 0; i < selectedBone->animations[currentAnimation].frames.size(); i++)
		updateRotations(selectedBone, selectedBone->animations[currentAnimation].frames[i].step, true);
}

void verifyBoneAnimationCounts(bone * pBone) {
	if (pBone == NULL) pBone = root;

	if (pBone->animations.size() < animations.size()) {
		for (unsigned i = pBone->animations.size(); i < animations.size(); i++) {
			bone::animation tempAnimation = {animations[i].name, animations[i].length};
			tempAnimation.frames.push_back((bone::keyFrame){0.0f, 0.0f, 0.0f, 1});
			pBone->animations.push_back(tempAnimation);
		}
	}

	while (pBone->animations.size() > animations.size()) pBone->animations.pop_back();
	for (unsigned i = 0; i < pBone->child.size(); i++) verifyBoneAnimationCounts(pBone->child[i]);
}

void sortBoneAnimationFrames(bone * pBone = NULL) {
	if (pBone == NULL) pBone = root;
	for (unsigned i = 0; i < pBone->animations.size(); i++) {
		bone::animation tempAnimation;
		tempAnimation.name = pBone->animations[i].name;
		while (pBone->animations[i].frames.size() > 0) {
			bone::keyFrame lowestFrame = pBone->animations[i].frames.front();
			for (unsigned j = 0; j < pBone->animations[i].frames.size(); j++) {
				if (pBone->animations[i].frames[j].step < lowestFrame.step) lowestFrame = pBone->animations[i].frames[j];
			}
			pBone->animations[i].frames.erase(pBone->animations[i].frames.begin()+pBone->animations[i].frameIndex(lowestFrame.step));
			tempAnimation.frames.push_back(lowestFrame);
		}
		pBone->animations[i] = tempAnimation;
	}
	for (unsigned i = 0; i < pBone->child.size(); i++) sortBoneAnimationFrames(pBone->child[i]);
}

void setKeyframe(bone * pBone = NULL) {
	if (pBone == NULL) pBone = root;

	int frameIndex = pBone->animations[currentAnimation].frameIndex(currentFrame);
	if (frameIndex == -1) {
		unsigned i = 0;
		for (; i < pBone->animations[currentAnimation].frames.size(); i++) if (currentFrame < pBone->animations[currentAnimation].frames[i].step) break;

		if ((pBone->xRot != pBone->animations[currentAnimation].frames[i].xRot) || (pBone->yRot != pBone->animations[currentAnimation].frames[i].yRot) ||
				(pBone->zRot != pBone->animations[currentAnimation].frames[i].zRot) || (pBone == selectedBone)) {
			bone::keyFrame newKeyFrame = (bone::keyFrame){pBone->xRot, pBone->yRot, pBone->zRot, currentFrame};
			pBone->animations[currentAnimation].frames.push_back(newKeyFrame);

		}
	} else {
		pBone->animations[currentAnimation].frames[frameIndex].xRot = pBone->xRot;
		pBone->animations[currentAnimation].frames[frameIndex].yRot = pBone->yRot;
		pBone->animations[currentAnimation].frames[frameIndex].zRot = pBone->zRot;
	}

	for (unsigned i = 0; i < pBone->child.size(); i++) setKeyframe(pBone->child[i]);
}

void deleteKeyframe(bone * pBone, unsigned frame) {
	int frameIndex = pBone->animations[currentAnimation].frameIndex(frame);
	if (frameIndex != -1) pBone->animations[currentAnimation].frames.erase(pBone->animations[currentAnimation].frames.begin()+frameIndex);
}

void setKeyframeCallback() {
	sortBoneAnimationFrames(); //Just to make sure they keyframes are sorted beforehand
	setKeyframe();
	sortBoneAnimationFrames();
	setAnimationMarks(selectedBone);
}

vec2 mouseMovedAmount() {
	if (mouseX() < MOUSE_MIDDLE_BORDER) {
		setMousePosition(screenWidth()-MOUSE_MIDDLE_BORDER, mouseY());
		lastMousePosition.x = mouseX();
	} else if (mouseX() > (int)screenWidth()-MOUSE_MIDDLE_BORDER) {
		setMousePosition(MOUSE_MIDDLE_BORDER, mouseY());
		lastMousePosition.x = mouseX();
	}
	if (mouseY() < MOUSE_MIDDLE_BORDER) {
		setMousePosition(mouseX(), screenHeight()-MOUSE_MIDDLE_BORDER);
		lastMousePosition.y = mouseY();
	} else if (mouseY() > (int)screenHeight()-MOUSE_MIDDLE_BORDER) {
		setMousePosition(mouseX(), MOUSE_MIDDLE_BORDER);
		lastMousePosition.y = mouseY();
	}
	return (vec2){{mouseX()-lastMousePosition.x}, {mouseY()-lastMousePosition.y}};
}

void drawArrow(float x, float y, float z, axisEnum axis) {
	disableDepthTesting();
	pushMatrix();
		float length = (DEFAULT_ZOOM/zoom)*10.0f;
		translateMatrix(x, y, z);
		rotateMatrix(180.0f, 0.0f, 1.0f, 0.0f);
		vec4 color;
		switch (axis) {
		case X_AXIS:
			translateMatrix(-length, 0.0f, 0.0f);
			if ((viewOrientation == TOP) || (viewOrientation == BOTTOM)) rotateMatrix(90.0f, 1.0f, 0.0f, 0.0f);
			color = (vec4){{0.0f}, {0.0f}, {1.0f}, {1.0f}};
			break;
		case Y_AXIS:
			translateMatrix(0.0f, length, 0.0f);
			rotateMatrix(-90.0f, 0.0f, 0.0f, 1.0f);
			if ((viewOrientation == LEFT) || (viewOrientation == RIGHT)) rotateMatrix(90.0f, 1.0f, 0.0f, 0.0f);
			color = (vec4){{1.0f}, {0.0f}, {0.0f}, {1.0f}};
			break;
		case Z_AXIS:
			translateMatrix(0.0f, 0.0f, -length);
			rotateMatrix(-90.0f, 0.0f, 1.0f, 0.0f);
			if ((viewOrientation == TOP) || (viewOrientation == BOTTOM)) rotateMatrix(90.0f, 1.0f, 0.0f, 0.0f);
			color = (vec4){{0.0f}, {1.0f}, {0.0f}, {1.0f}};
			break;
		}

		arrowShader->use();
		arrowShader->setUniform16(MODELVIEW_LOCATION, getMatrix(MODELVIEW_MATRIX));
		arrowShader->setUniform16(PROJECTION_LOCATION, getMatrix(PROJECTION_MATRIX));
		arrowShader->setUniform4(TEXSAMPLER_LOCATION, color.r, color.g, color.b, color.a);
		arrowShader->setUniform1(EXTRA0_LOCATION, length);

		glBindVertexArray(arrowVao);
		glDrawArrays(GL_LINES, 0, 6);
		glBindVertexArray(0);
	popMatrix();
	enableDepthTesting();
}

void drawBox(vec2 startPosition) {
	disableDepthTesting();
	pushMatrix();
		boxShader->use();
		boxShader->setUniform16(MODELVIEW_LOCATION, getMatrix(MODELVIEW_MATRIX));
		boxShader->setUniform16(PROJECTION_LOCATION, getMatrix(PROJECTION_MATRIX));
		boxShader->setUniform4(TEXSAMPLER_LOCATION, 1.0f, 1.0f, 1.0f, 1.0f);
		boxShader->setUniform2(EXTRA0_LOCATION, startPosition.x, screenHeight()-startPosition.y);
		boxShader->setUniform2(EXTRA1_LOCATION, (float)mouseX(), (float)mouseY());

		glBindVertexArray(boxVao);
		glDrawArrays(GL_LINE_LOOP, 0, 4);
		glBindVertexArray(0);
	popMatrix();
	enableDepthTesting();
}

void drawRing(float x, float y, float z, axisEnum axis) {
	disableDepthTesting();
	pushMatrix();
		float scale = (DEFAULT_ZOOM/zoom)*10.0f;
		translateMatrix(x, y, z);
		rotateMatrix(180.0f, 0.0f, 1.0f, 0.0f);
		vec4 color;
		switch (axis) {
		case X_AXIS:
			rotateMatrix(-90.0f, 0.0f, 1.0f, 0.0f);
			color = (vec4){{0.0f}, {0.0f}, {1.0f}, {1.0f}};
			break;
		case Y_AXIS:
			rotateMatrix(-90.0f, 1.0f, 0.0f, 0.0f);
			color = (vec4){{1.0f}, {0.0f}, {0.0f}, {1.0f}};
			break;
		case Z_AXIS:
			color = (vec4){{0.0f}, {1.0f}, {0.0f}, {1.0f}};
			break;
		}

		ringShader->use();
		ringShader->setUniform16(MODELVIEW_LOCATION, getMatrix(MODELVIEW_MATRIX));
		ringShader->setUniform16(PROJECTION_LOCATION, getMatrix(PROJECTION_MATRIX));
		ringShader->setUniform4(TEXSAMPLER_LOCATION, color.r, color.g, color.b, color.a);
		ringShader->setUniform1(EXTRA0_LOCATION, scale);

		glBindVertexArray(ringVao);
		glDrawArrays(GL_LINE_LOOP, 0, 20);
		glBindVertexArray(0);
	popMatrix();
	enableDepthTesting();
}

void handleControlPressed(bool * showArrow, bool * showArrowParent, axisEnum * arrowAxis) {
	if (timeSinceShortcutPressed < SHORTCUT_PRESS_DELAY) timeSinceShortcutPressed += compensation(); else {
		if (keyPressed('o')) {
			flagExecuteOpenFile();
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed('w')) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wireframeToggleButton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wireframeToggleButton)));
			timeSinceShortcutPressed = 0.0f;

		} else if (keyPressed(ZERO_KEYCODE) || keyPressed(NUMPAD_ZERO_KEYCODE)) {
			setViewOrientation(NULL, &viewOrientationArr[FREE]);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(ZERO_KEYCODE+1) || keyPressed(NUMPAD_ZERO_KEYCODE+1)) {
			setViewOrientation(NULL, &viewOrientationArr[TOP]);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(ZERO_KEYCODE+2) || keyPressed(NUMPAD_ZERO_KEYCODE+2)) {
			setViewOrientation(NULL, &viewOrientationArr[BOTTOM]);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(ZERO_KEYCODE+3) || keyPressed(NUMPAD_ZERO_KEYCODE+3)) {
			setViewOrientation(NULL, &viewOrientationArr[LEFT]);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(ZERO_KEYCODE+4) || keyPressed(NUMPAD_ZERO_KEYCODE+4)) {
			setViewOrientation(NULL, &viewOrientationArr[RIGHT]);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(ZERO_KEYCODE+5) || keyPressed(NUMPAD_ZERO_KEYCODE+5)) {
			setViewOrientation(NULL, &viewOrientationArr[FRONT]);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(ZERO_KEYCODE+6) || keyPressed(NUMPAD_ZERO_KEYCODE+6)) {
			setViewOrientation(NULL, &viewOrientationArr[BACK]);
			timeSinceShortcutPressed = 0.0f;

		} else if (keyPressed(UP_KEYCODE)) {
			scaleMatrix(1.1, 1.1, 1.1);
			zoom *= 1.1;
			timeSinceShortcutPressed = SHORTCUT_PRESS_DELAY/2.0f;
		} else if (keyPressed(DOWN_KEYCODE)) {
			scaleMatrix(0.9, 0.9, 0.9);
			zoom *= 0.9;
			timeSinceShortcutPressed = SHORTCUT_PRESS_DELAY/2.0f;
		} else if (keyPressed(RIGHT_KEYCODE)) {
			setBoneScale(boneScale+0.1f);
			timeSinceShortcutPressed = 0.0f;
		} else if (keyPressed(LEFT_KEYCODE)) {
			setBoneScale(boneScale-0.1f);
			timeSinceShortcutPressed = 0.0f;

		} else if (mode == SKELETON_MODE) {
			if (keyPressed('b')) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(boneCreationToggleButton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(boneCreationToggleButton)));
				timeSinceShortcutPressed = 0.0f;
			} else if (keyPressed('k')) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(skinningToggleButton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(skinningToggleButton)));
				timeSinceShortcutPressed = 0.0f;

			} else if (keyPressed(SHIFT_KEYCODE)) {
				float amountMoved = -(float(mouseMovedAmount().y)/3.0f)*compensation();
				if (keyPressed('z')) {
					selectedBone->endZ += amountMoved;
					updateBoneCoords(selectedBone);
					*showArrow = true;
					*showArrowParent = false;
					*arrowAxis = Z_AXIS;
				} else if (keyPressed('x')) {
					selectedBone->endX += amountMoved;
					updateBoneCoords(selectedBone);
					*showArrow = true;
					*showArrowParent = false;
					*arrowAxis = X_AXIS;
				} else if (keyPressed('s')) {
					selectedBone->endY += amountMoved;
					updateBoneCoords(selectedBone);
					*showArrow = true;
					*showArrowParent = false;
					*arrowAxis = Y_AXIS;
				}

			} else if (keyPressed(ALT_KEYCODE)) {
				float amountMoved = -(float(mouseMovedAmount().y)/3.0f)*compensation();
				if (keyPressed('z')) {
					if (selectedBone == root) {
						selectedBone->z += amountMoved;
						updateBoneCoords(selectedBone);
					} else {
						selectedBone->parent->endZ += amountMoved;
						updateBoneCoords(selectedBone->parent);
					}
					*showArrow = true;
					*showArrowParent = true;
					*arrowAxis = Z_AXIS;
				} else if (keyPressed('x')) {
					if (selectedBone == root) {
						selectedBone->x += amountMoved;
						updateBoneCoords(selectedBone);
					} else {
						selectedBone->parent->endX += amountMoved;
						updateBoneCoords(selectedBone->parent);
					}
					*showArrow = true;
					*showArrowParent = true;
					*arrowAxis = X_AXIS;
				} else if (keyPressed('s')) {
					if (selectedBone == root) {
						selectedBone->y += amountMoved;
						updateBoneCoords(selectedBone);
					} else {
						selectedBone->parent->endY += amountMoved;
						updateBoneCoords(selectedBone->parent);
					}
					*showArrow = true;
					*showArrowParent = true;
					*arrowAxis = Y_AXIS;
				}
			}
		} else if (mode == ANIMATION_MODE) {
			if (keyPressed('p')) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playAnimationToggleButton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(playAnimationToggleButton)));
				timeSinceShortcutPressed = 0.0f;
			}
		}
	}
}

void handleAltPressed(bool * showRing, axisEnum * ringAxis) {
	if (playAnimation) return;

	float amountMoved = -(float(mouseMovedAmount().y)/3.0f)*compensation();
	if (keyPressed('z')) {
		*showRing = true;
		*ringAxis = Z_AXIS;

		if (amountMoved != 0.0f) {
			selectedBone->zRot += amountMoved;

			if (autoKeyEnabled) {
				int frameIndex = selectedBone->animations[currentAnimation].frameIndex(currentFrame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){selectedBone->xRot, selectedBone->yRot, selectedBone->zRot, currentFrame};
					selectedBone->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					selectedBone->animations[currentAnimation].frames[frameIndex].zRot = selectedBone->zRot;
				}
			}

			updateRotations(selectedBone, currentFrame);
			sortBoneAnimationFrames();
			setAnimationMarks(selectedBone);
		}
	} else if (keyPressed('x')) {
		*showRing = true;
		*ringAxis = X_AXIS;

		if (amountMoved != 0.0f) {
			selectedBone->xRot += amountMoved;

			if (autoKeyEnabled) {
				int frameIndex = selectedBone->animations[currentAnimation].frameIndex(currentFrame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){selectedBone->xRot, selectedBone->yRot, selectedBone->zRot, currentFrame};
					selectedBone->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					selectedBone->animations[currentAnimation].frames[frameIndex].xRot = selectedBone->xRot;
				}
			}

			updateRotations(selectedBone, currentFrame);
			sortBoneAnimationFrames();
			setAnimationMarks(selectedBone);
		}
	} else if (keyPressed('s')) {
		*showRing = true;
		*ringAxis = Y_AXIS;

		if (amountMoved != 0.0f) {
			selectedBone->yRot += amountMoved;

			if (autoKeyEnabled) {
				int frameIndex = selectedBone->animations[currentAnimation].frameIndex(currentFrame);
				if (frameIndex == -1) {
					bone::keyFrame tempFrame = (bone::keyFrame){selectedBone->xRot, selectedBone->yRot, selectedBone->zRot, currentFrame};
					selectedBone->animations[currentAnimation].frames.push_back(tempFrame);
				} else {
					selectedBone->animations[currentAnimation].frames[frameIndex].yRot = selectedBone->yRot;
				}
			}

			updateRotations(selectedBone, currentFrame);
			sortBoneAnimationFrames();
			setAnimationMarks(selectedBone);
		}
	}
}

void handleBoneCreation() {
	if (mouseLeft() && boneCreationEnabled) {
		if (!creatingBone) {
			creatingBone = true;
			if ((root == NULL) || (selectedBone == NULL)) {
				root = new bone;
				initBone(root);
				root->name = "root";
				double x, y, z, mvMat[16], pMat[16];
				int viewport[4] = {0, 0, screenWidth(), screenHeight()};
				setMatrix(MODELVIEW_MATRIX);
				pushMatrix();
					copyMatrix(IDENTITY_MATRIX, MODELVIEW_MATRIX);
					translateMatrix(screenWidth()/2.0f, screenHeight()/2.0f, -500.0f);
					scaleMatrix(zoom, -zoom, zoom);
					translateMatrix(viewTranslation.x, viewTranslation.y, 0.0f);
					rotateMatrix(xRotation, 1.0f, 0.0f, 0.0f);
					rotateMatrix(yRotation, 0.0f, 1.0f, 0.0f);
					for (int i = 0; i < 16; i++) {
						mvMat[i] = getMatrix(MODELVIEW_MATRIX)[i];
						pMat[i] = getMatrix(PROJECTION_MATRIX)[i];
					}
					gluUnProject(mouseX(), screenHeight()-mouseY(), 0, mvMat, pMat, viewport, &x, &y, &z);
				popMatrix();
				switch (viewOrientation) {
				case TOP:
				case BOTTOM:
					root->x = x;
					root->y = 0.0f;
					root->z = z;
					break;
				case LEFT:
				case RIGHT:
					root->x = 0.0f;
					root->y = y;
					root->z = z;
					break;
				case FRONT:
				case BACK:
					root->x = x;
					root->y = y;
					root->z = 0.0f;
					break;
				default: break;
				}
				selectedBone = root;

				boneIteratorAssociations.push_back((boneIteratorAssociation){selectedBone});
				gtk_tree_store_append(boneStore, &(boneIteratorAssociations.back().iterator), NULL);
			} else {
				bone * parentBone = selectedBone;
				selectedBone = new bone;
				parentBone->child.push_back(selectedBone);
				initBone(selectedBone, parentBone);
				selectedBone->x = parentBone->x+parentBone->endX;
				selectedBone->y = parentBone->y+parentBone->endY;
				selectedBone->z = parentBone->z+parentBone->endZ;

				unsigned i;
				for (i = 0; i < boneIteratorAssociations.size(); i++) {
					if (boneIteratorAssociations[i].pBone == selectedBone->parent) break;
				}

				boneIteratorAssociations.push_back((boneIteratorAssociation){selectedBone});
				gtk_tree_store_append(boneStore, &(boneIteratorAssociations.back().iterator), &(boneIteratorAssociations[i].iterator));
			}
			gtk_tree_store_set(boneStore, &(boneIteratorAssociations.back().iterator), 0, selectedBone->name.c_str(), -1);

			GtkTreePath * tempPath = gtk_tree_model_get_path(GTK_TREE_MODEL(boneStore), &(boneIteratorAssociations.back().iterator));
			gtk_tree_view_expand_to_path(GTK_TREE_VIEW(boneView), tempPath);
			gtk_tree_path_free(tempPath);
			gtk_tree_selection_select_iter(boneSelect, &(boneIteratorAssociations.back().iterator));
			verifyBoneAnimationCounts(selectedBone);
			setRotationLimitValues(selectedBone);

			SuperMaximo::wait(200);
		} else {
			creatingBone = false;
		}
	}
}

void handleBoneCompletion() {
	if (creatingBone) {
		double x, y, z, mvMat[16], pMat[16];
		int viewport[4] = {0, 0, screenWidth(), screenHeight()};
		setMatrix(MODELVIEW_MATRIX);
		pushMatrix();
			copyMatrix(IDENTITY_MATRIX, MODELVIEW_MATRIX);
			translateMatrix(screenWidth()/2.0f, screenHeight()/2.0f, -500.0f);
			scaleMatrix(zoom, -zoom, zoom);
			translateMatrix(viewTranslation.x, viewTranslation.y, 0.0f);
			rotateMatrix(xRotation, 1.0f, 0.0f, 0.0f);
			rotateMatrix(yRotation, 0.0f, 1.0f, 0.0f);
			for (int i = 0; i < 16; i++) {
				mvMat[i] = getMatrix(MODELVIEW_MATRIX)[i];
				pMat[i] = getMatrix(PROJECTION_MATRIX)[i];
			}
			gluUnProject(mouseX(), screenHeight()-mouseY(), 0, mvMat, pMat, viewport, &x, &y, &z);
		popMatrix();
		switch (viewOrientation) {
		case TOP:
		case BOTTOM:
			selectedBone->endX = x-selectedBone->x;
			selectedBone->endY = 0.0f;
			selectedBone->endZ = z-selectedBone->z;
			break;
		case LEFT:
		case RIGHT:
			selectedBone->endX = 0.0f;
			selectedBone->endY = y-selectedBone->y;
			selectedBone->endZ = z-selectedBone->z;
			break;
		case FRONT:
		case BACK:
			selectedBone->endX = x-selectedBone->x;
			selectedBone->endY = y-selectedBone->y;
			selectedBone->endZ = 0.0f;
			break;
		default: break;
		}
		if (keyPressed(27) || mouseRight()) {
			creatingBone = false;
			if (keyPressed(27)) {
				bone * tempBone = selectedBone;
				selectedBone = selectedBone->parent;
				deleteBone(tempBone);
			}
		}
	}
}

void selectVertices(vec2 boxStartPosition) {
	if ((loadedModel == NULL) || (selectedBone == NULL)) return;

	double mvMat[16], pMat[16];
	setMatrix(MODELVIEW_MATRIX);
	pushMatrix();
		copyMatrix(IDENTITY_MATRIX, MODELVIEW_MATRIX);
		translateMatrix(screenWidth()/2.0f, screenHeight()/2.0f, -500.0f);
		scaleMatrix(zoom, -zoom, zoom);
		translateMatrix(viewTranslation.x, viewTranslation.y, 0.0f);
		rotateMatrix(xRotation, 1.0f, 0.0f, 0.0f);
		rotateMatrix(yRotation, 0.0f, 1.0f, 0.0f);
		for (short i = 0; i < 16; i++) {
			mvMat[i] = getMatrix(MODELVIEW_MATRIX)[i];
			pMat[i] = getMatrix(PROJECTION_MATRIX)[i];
		}
	popMatrix();

	glBindBuffer(GL_ARRAY_BUFFER, *modelVbo);
	for (unsigned i = 0; i < loadedModel->triangles()->size(); i++) {
		for (short j = 0; j < 3; j++) {
			double x, y, z;
			int viewport[4] = {0, 0, screenWidth(), screenHeight()};

			int loX, hiX, loY, hiY;
			if (boxStartPosition.x > mouseX()) {
				loX = mouseX();
				hiX = boxStartPosition.x;
			} else {
				loX = boxStartPosition.x;
				hiX = mouseX();
			}
			if (screenHeight()-boxStartPosition.y > mouseY()) {
				hiY = screenHeight()-mouseY();
				loY = boxStartPosition.y;
			} else {
				hiY = boxStartPosition.y;
				loY = screenHeight()-mouseY();
			}

			gluProject((*(loadedModel->triangles()))[i].coords[j].x, (*(loadedModel->triangles()))[i].coords[j].y, (*(loadedModel->triangles()))[i].coords[j].z, mvMat,
					pMat, viewport, &x, &y, &z);

			if ((x <= hiX) && (x >= loX) && (y <= hiY) && (y >= loY)) {
				if (keyPressed(SHIFT_KEYCODE)) {
					GLfloat data = -1.0f;
					glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
					/*for (unsigned k = 0; k < selectedBone->vertices.size(); k++) {
						if (selectedBone->vertices[k] == &((*(loadedModel->triangles()))[i].coords[j])) {
							selectedBone->vertices.erase(selectedBone->vertices.begin()+k);
							break;
						}
					}*/
				} else {
					GLfloat data = selectedBone->id;
					glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
					//selectedBone->vertices.push_back(&((*(loadedModel->triangles()))[i].coords[j]));
				}
			} else {
				if ((mouseX() <= x+3.0f) && (mouseX() >= x-3.0f) && ((screenHeight()-mouseY()) <= y+3.0f) &&((screenHeight()-mouseY()) >= y-3.0f)) {
					if (keyPressed(SHIFT_KEYCODE)) {
						GLfloat data = -1.0f;
						glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
						/*for (unsigned k = 0; k < selectedBone->vertices.size(); k++) {
							if (selectedBone->vertices[k] == &((*(loadedModel->triangles()))[i].coords[j])) {
								selectedBone->vertices.erase(selectedBone->vertices.begin()+k);
								break;
							}
						}*/
					} else {
						GLfloat data = selectedBone->id;
						glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLfloat)*((i*24*3)+(j*24)+23), sizeof(GLfloat), &data);
						//selectedBone->vertices.push_back(&((*(loadedModel->triangles()))[i].coords[j]));
					}
				}
			}
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void handleSkinning(bool * showBox, vec2 * returnBoxStartPosition) {
	static bool selecting = false;
	static vec2 boxStartPosition = (vec2){{-1.0f}, {-1.0f}};

	if (mouseLeft()) {
		if (!selecting) {
			boxStartPosition.x = mouseX();
			boxStartPosition.y = screenHeight()-mouseY();
			selecting = true;
		}
	} else {
		if (selecting) {
			selectVertices(boxStartPosition);
			selecting = false;
		}
	}

	*showBox = selecting;
	*returnBoxStartPosition = boxStartPosition;
}

void applyBoneTransforms(bone * startBone) {
	if (startBone->parent != NULL) applyBoneTransforms(startBone->parent);

	translateMatrix(startBone->x, startBone->y, startBone->z);
	rotateMatrix(startBone->xRot, 1.0f, 0.0f, 0.0f);
	rotateMatrix(startBone->yRot, 0.0f, 1.0f, 0.0f);
	rotateMatrix(startBone->zRot, 0.0f, 0.0f, 1.0f);
	translateMatrix(-startBone->x, -startBone->y, -startBone->z);
}

void getBoneModelviewMatrices(matrix4d * matrixArray, bone * pBone = NULL) {
	if (pBone == NULL) pBone = root;

	pushMatrix();
		translateMatrix(pBone->x, pBone->y, pBone->z);
		rotateMatrix(pBone->xRot, 1.0f, 0.0f, 0.0f);
		rotateMatrix(pBone->yRot, 0.0f, 1.0f, 0.0f);
		rotateMatrix(pBone->zRot, 0.0f, 0.0f, 1.0f);
		translateMatrix(-pBone->x, -pBone->y, -pBone->z);
		matrixArray[pBone->id] = getMatrix(MODELVIEW_MATRIX);

		for (unsigned i = 0; i < pBone->child.size(); i++) getBoneModelviewMatrices(matrixArray, pBone->child[i]);
	popMatrix();
}

void sendBoneModelviewMatrixUniform() {
	matrix4d matrixArray[boneList.size()];
	getBoneModelviewMatrices(matrixArray);
	animationShader->use();
	animationShader->setUniform16(EXTRA0_LOCATION, (float*)matrixArray, boneList.size());
}

gboolean glLoop(void*) {
	if (closeClicked()) {
		gtk_main_quit();
		return false;
	}

	//for (unsigned i = 0; i < 320; i++) if (keyPressed(i)) cout << i << endl;

	bool showArrow = false, showArrowParent, showRing = false;
	axisEnum axis;
	if (keyPressed(CONTROL_KEYCODE)) handleControlPressed(&showArrow, &showArrowParent, &axis); else
		if (keyPressed(ALT_KEYCODE) && (mode == ANIMATION_MODE)) handleAltPressed(&showRing, &axis);

	if (mouseWheelUp()) {
		scaleMatrix(0.9, 0.9, 0.9);
		zoom *= 0.9;
	} else if (mouseWheelDown()) {
		scaleMatrix(1.1, 1.1, 1.1);
		zoom *= 1.1;
	}

	if (((mouseMiddle() || (keyPressed(CONTROL_KEYCODE) && mouseLeft())) && !keyPressed(ALT_KEYCODE) && !keyPressed(SHIFT_KEYCODE)) && (viewOrientation == FREE)) {
		vec2 amountMoved = mouseMovedAmount();
		xRotation += (float(amountMoved.y)/3.0f)*compensation();
		yRotation += (float(amountMoved.x)/3.0f)*compensation();
	} else handleBoneCreation();

	handleBoneCompletion();

	if (mouseRight()) {
		vec2 amountMoved = mouseMovedAmount();
		viewTranslation.x += amountMoved.x*0.3;
		viewTranslation.y -= amountMoved.y*0.3;
	} else if (keyPressed(SPACEBAR_KEYCODE)) {
		viewTranslation = (vec2){{0.0f}, {0.0f}};
	}

	if (keyPressed(127)) {
		if (mode == ANIMATION_MODE) {
			deleteKeyframe(selectedBone, currentFrame);
			setAnimationMarks(selectedBone);
			setBoneRotations(currentFrame);
		} else if ((selectedBone != NULL) && (mode == SKELETON_MODE)) {
			bone * tempBone = selectedBone;
			selectedBone = selectedBone->parent;
			deleteBone(tempBone);
			SuperMaximo::wait(200);
		}
	}

	bool showBox = false;
	vec2 boxStartPosition;
	if (skinningEnabled) handleSkinning(&showBox, &boxStartPosition);

	if (playAnimation) {
		currentFrame++;
		if (currentFrame > animations[currentAnimation].length) currentFrame = 1;
		gtk_range_set_value(GTK_RANGE(timeline), currentFrame);
		setCurrentFrameFromScale();
	}

	copyMatrix(ORTHOGRAPHIC_MATRIX, PROJECTION_MATRIX);
	pushMatrix();
		translateMatrix(viewTranslation.x, viewTranslation.y, 0.0f);
		rotateMatrix(xRotation, 1.0f, 0.0f, 0.0f);
		rotateMatrix(yRotation, 0.0f, 1.0f, 0.0f);

		if ((selectedBone != NULL) && (mode == SKELETON_MODE)) {
			skeletonShader->use();
			skeletonShader->setUniform1(EXTRA1_LOCATION, (float)selectedBone->id);
		}

		if (wireframeModeEnabled) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		if (loadedModel != NULL) {
			if (mode == ANIMATION_MODE) sendBoneModelviewMatrixUniform();
			loadedModel->draw(0.0f, 0.0f, 0.0f);

			if (skinningEnabled) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
				glPointSize(5);
				skeletonShader->setUniform1(EXTRA2_LOCATION, 1.0f);
				loadedModel->draw(0.0f, 0.0f, 0.0f);
				skeletonShader->setUniform1(EXTRA2_LOCATION, 0.0f);
			}
		}
		if (wireframeModeEnabled || skinningEnabled) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		drawBone(root);

		if (showArrow) {
			pushMatrix();
				applyBoneTransforms(selectedBone);
				if (showArrowParent) drawArrow(selectedBone->x, selectedBone->y, selectedBone->z, axis); else
					drawArrow(selectedBone->x+selectedBone->endX, selectedBone->y+selectedBone->endY, selectedBone->z+selectedBone->endZ, axis);
			popMatrix();
		} else if (showRing) {
			pushMatrix();
				applyBoneTransforms(selectedBone);
				drawRing(selectedBone->x, selectedBone->y, selectedBone->z, axis);
			popMatrix();
		}
	popMatrix();

	if (showBox) {
		pushMatrix();
			copyMatrix(IDENTITY_MATRIX, MODELVIEW_MATRIX);
			drawBox(boxStartPosition);
		popMatrix();
	}

	lastMousePosition = (vec2){{mouseX()}, {mouseY()}};
	refreshScreen();

	if (executeOpenFile) {
		openFile();
		executeOpenFile = false;
	}

	return true;
}

void removeExcessKeyframes(bone * pBone = NULL) {
	if (pBone == NULL) pBone = root;

	for (unsigned i = 0; i < pBone->animations[currentAnimation].frames.size(); i++)
		if (pBone->animations[currentAnimation].frames[i].step > animations[currentAnimation].length)
			pBone->animations[currentAnimation].frames.erase(pBone->animations[currentAnimation].frames.begin()+i);

	for (unsigned i = 0; i < pBone->child.size(); i++) removeExcessKeyframes(pBone->child[i]);
}

void updateAnimationLength() {
	animations[currentAnimation].length = gtk_spin_button_get_value(GTK_SPIN_BUTTON(animationLengthSpinButton));
	gtk_range_set_range(GTK_RANGE(timeline), 1, animations[currentAnimation].length);
	if ((unsigned)atoi(gtk_entry_get_text(GTK_ENTRY(timelineJumpEntry))) > animations[currentAnimation].length) {
		stringstream stream(stringstream::in | stringstream::out);
		stream.setf(ios::fixed, ios::floatfield);
		stream << animations[currentAnimation].length;
		gtk_entry_set_text(GTK_ENTRY(timelineJumpEntry), stream.str().c_str());
	}
	if (currentFrame > animations[currentAnimation].length) currentFrame = animations[currentAnimation].length;
	removeExcessKeyframes();
	setAnimationMarks(selectedBone);
}

void renameAnimation() {
	animations[currentAnimation].name = gtk_entry_get_text(GTK_ENTRY(animationNameEntry));
	if (animations[currentAnimation].name == "") {
		stringstream stream(stringstream::in | stringstream::out);
		stream.setf(ios::fixed, ios::floatfield);
		stream << currentAnimation;
		animations[currentAnimation].name = "animation"+stream.str();
		gtk_entry_set_text(GTK_ENTRY(animationNameEntry), animations[currentAnimation].name.c_str());
	}
}

void timelineJump() {
	currentFrame = atoi(gtk_entry_get_text(GTK_ENTRY(timelineJumpEntry)));
	if (currentFrame < 1) {
		currentFrame = 1;
		gtk_entry_set_text(GTK_ENTRY(timelineJumpEntry), "1");
	} else if (currentFrame > animations[currentAnimation].length) {
		currentFrame = animations[currentAnimation].length;
		stringstream stream(stringstream::in | stringstream::out);
		stream.setf(ios::fixed, ios::floatfield);
		stream << currentFrame;
		gtk_entry_set_text(GTK_ENTRY(timelineJumpEntry), stream.str().c_str());
	}
	gtk_range_set_value(GTK_RANGE(timeline), currentFrame);

	setBoneRotations(currentFrame);
}

void setCurrentFrameFromScale() {
	currentFrame = gtk_range_get_value(GTK_RANGE(timeline));

	stringstream stream(stringstream::in | stringstream::out);
	stream.setf(ios::fixed, ios::floatfield);
	stream << currentFrame;
	gtk_entry_set_text(GTK_ENTRY(timelineJumpEntry), stream.str().c_str());

	setBoneRotations(currentFrame);
}

void selectAnimation() {
	currentAnimation = gtk_spin_button_get_value(GTK_SPIN_BUTTON(animationSelectSpinButton));
	setAnimationMarks(selectedBone);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(animationLengthSpinButton), animations[currentAnimation].length);
	gtk_entry_set_text(GTK_ENTRY(timelineJumpEntry), "1");
	timelineJump();
}

void addAnimation() {
	stringstream stream(stringstream::in | stringstream::out);
	stream.setf(ios::fixed, ios::floatfield);
	stream << animations.size()-1;
	animations.push_back((animationDetail){"animation"+stream.str(), 60});
	verifyBoneAnimationCounts(root);
	updateAnimationSpinButtonRange();
	currentAnimation = animations.size()-1;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(animationSelectSpinButton), currentAnimation);
	selectAnimation();
}

void deleteAnimation() {
	animations.erase(animations.begin()+currentAnimation);
	for (unsigned i = 0; i < boneList.size(); i++) boneList[i]->animations.erase(boneList[i]->animations.begin()+currentAnimation);
	if (currentAnimation > 0) currentAnimation--;
	if (animations.size() == 0) addAnimation(); else {
		updateAnimationSpinButtonRange();
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(animationSelectSpinButton), currentAnimation);
		selectAnimation();
	}
}

void updateAnimationSpinButtonRange() {
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(animationSelectSpinButton), 0, animations.size()-1);
}

void switchMode(GtkWidget *, bool * recreateWindow) {
	if (selectedBone == NULL) return;

	if (mode == SKELETON_MODE) {
		mode = ANIMATION_MODE;
		if (boneCreationEnabled) {
			g_signal_handler_block(boneCreationToggleButton, boneCreationToggleHandler);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(boneCreationToggleButton), 0);
			g_signal_handler_unblock(boneCreationToggleButton, boneCreationToggleHandler);
			boneCreationEnabled = false;
		}
		if (skinningEnabled) {
			g_signal_handler_block(skinningToggleButton, skinningToggleHandler);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(skinningToggleButton), 0);
			g_signal_handler_unblock(skinningToggleButton, skinningToggleHandler);
			skinningEnabled = false;
		}
		gtk_widget_hide(boneCreationToggleButton);
		gtk_widget_hide(skinningToggleButton);
		gtk_widget_show_all(animationWindow);
		gtk_button_set_label(GTK_BUTTON(switchModeButton), "<-  Skeleton mode");

		verifyBoneAnimationCounts();
		setAnimationMarks(selectedBone);
		animationShader->bind();
	} else {
		mode = SKELETON_MODE;
		if (*recreateWindow) animationWindow = createAnimationWindow();
		gtk_widget_hide(animationWindow);
		gtk_widget_show_all(boneWindow);
		gtk_button_set_label(GTK_BUTTON(switchModeButton), "Animation mode  ->");
		resetBoneRotations();
		skeletonShader->bind();
	}
	if (playAnimation) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(playAnimationToggleButton), 0);
	currentFrame = 1;
}

GtkWidget * createToolsWindow() {
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL), * button, * grid = gtk_grid_new();
	gtk_window_set_title(GTK_WINDOW(window), "Tools");
	gtk_window_set_resizable(GTK_WINDOW(window), false);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	int row = 1;

	button = gtk_button_new_with_label("New");
	g_signal_connect(button, "clicked", G_CALLBACK(resetAll), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, 1, row, 3, 1);
	row++;

	button = gtk_button_new_with_label("Import file");
	g_signal_connect(button, "clicked", G_CALLBACK(flagExecuteOpenFile), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, 1, row, 3, 1);
	row++;

	button = gtk_button_new_with_label("Save .smo");
	g_signal_connect(button, "clicked", G_CALLBACK(exportSmo), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, 1, row, 1, 1);
	button = gtk_button_new_with_label("Save .smm");
	g_signal_connect(button, "clicked", G_CALLBACK(exportSmmCallback), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, 1, row+1, 1, 1);
	button = gtk_button_new_with_label("Save .sms");
	g_signal_connect(button, "clicked", G_CALLBACK(exportSmsCallback), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, 2, row, 1, 1);
	button = gtk_button_new_with_label("Save .sma");
	g_signal_connect(button, "clicked", G_CALLBACK(exportSmaCallback), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, 2, row+1, 1, 1);
	row += 2;

	GtkWidget * label = gtk_label_new("");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	wireframeToggleButton = gtk_toggle_button_new_with_label("Wireframe mode");
	g_signal_connect(wireframeToggleButton, "toggled", G_CALLBACK(toggleWireframeMode), NULL);
	gtk_grid_attach(GTK_GRID(grid), wireframeToggleButton, 1, row, 3, 1);
	row++;

	label = gtk_label_new("");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;
	label = gtk_label_new("View orientation");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	viewToggleButton[TOP] = gtk_toggle_button_new_with_label("Top");
	viewToggleHandler[TOP] = g_signal_connect(viewToggleButton[TOP], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[TOP]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[TOP], 1, row, 1, 1);

	viewToggleButton[BOTTOM] = gtk_toggle_button_new_with_label("Bottom");
	viewToggleHandler[BOTTOM] = g_signal_connect(viewToggleButton[BOTTOM], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[BOTTOM]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[BOTTOM], 2, row, 1, 1);
	row++;

	viewToggleButton[LEFT] = gtk_toggle_button_new_with_label("Left");
	viewToggleHandler[LEFT] = g_signal_connect(viewToggleButton[LEFT], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[LEFT]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[LEFT], 1, row, 1, 1);

	viewToggleButton[RIGHT] = gtk_toggle_button_new_with_label("Right");
	viewToggleHandler[RIGHT] = g_signal_connect(viewToggleButton[RIGHT], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[RIGHT]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[RIGHT], 2, row, 1, 1);
	row++;

	viewToggleButton[FRONT] = gtk_toggle_button_new_with_label("Front");
	viewToggleHandler[FRONT] = g_signal_connect(viewToggleButton[FRONT], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[FRONT]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[FRONT], 1, row, 1, 1);

	viewToggleButton[BACK] = gtk_toggle_button_new_with_label("Back");
	viewToggleHandler[BACK] = g_signal_connect(viewToggleButton[BACK], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[BACK]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[BACK], 2, row, 1, 1);
	row++;

	viewToggleButton[FREE] = gtk_toggle_button_new_with_label("Free view");
	viewToggleHandler[FREE] = g_signal_connect(viewToggleButton[FREE], "toggled", G_CALLBACK(setViewOrientation), &viewOrientationArr[FREE]);
	gtk_grid_attach(GTK_GRID(grid), viewToggleButton[FREE], 1, row, 3, 1);
	row++;

	label = gtk_label_new("");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	switchModeButton = gtk_button_new_with_label("Animation mode  ->");
	g_signal_connect(switchModeButton, "clicked", G_CALLBACK(switchMode), &falseBool);
	gtk_grid_attach(GTK_GRID(grid), switchModeButton, 1, row, 3, 1);
	row++;

	gtk_container_add(GTK_CONTAINER(window), grid);
	return window;
}

GtkWidget * createBoneWindow() {
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL), * grid = gtk_grid_new(), * label;
	gtk_window_set_title(GTK_WINDOW(window), "Bones");
	gtk_window_set_resizable(GTK_WINDOW(window), false);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	int row = 1;

	boneCreationToggleButton = gtk_toggle_button_new_with_label("Create bones");
	boneCreationToggleHandler = g_signal_connect(boneCreationToggleButton, "toggled", G_CALLBACK(toggleBoneCreation), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneCreationToggleButton, 1, row, 3, 1);
	row++;

	skinningToggleButton = gtk_toggle_button_new_with_label("Skin bones");
	skinningToggleHandler = g_signal_connect(skinningToggleButton, "toggled", G_CALLBACK(toggleSkinning), NULL);
	gtk_grid_attach(GTK_GRID(grid), skinningToggleButton, 1, row, 3, 1);
	row++;

	label = gtk_label_new(" ");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	label = gtk_label_new("Bone scale:");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	boneScaleSpinButton = gtk_spin_button_new_with_range(0.0, 10.0, 0.1);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneScaleSpinButton), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneScaleSpinButton), 1.0);
	g_signal_connect(G_OBJECT(boneScaleSpinButton), "value-changed", G_CALLBACK(updateBoneScale), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneScaleSpinButton, 1, row, 3, 1);
	row++;

	label = gtk_label_new(" ");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	label = gtk_label_new("Bone name:");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	boneNameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(boneNameEntry), "");
	gtk_entry_set_width_chars(GTK_ENTRY(boneNameEntry), 4);
	gtk_entry_set_max_length(GTK_ENTRY(boneNameEntry), 8);
	g_signal_connect(G_OBJECT(boneNameEntry), "activate", G_CALLBACK(renameBone), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneNameEntry, 1, row, 3, 1);
	row++;

	label = gtk_label_new(" ");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	label = gtk_label_new("Bone rotation limits");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	label = gtk_label_new("Upper");
	gtk_grid_attach(GTK_GRID(grid), label, 2, row, 1, 1);
	label = gtk_label_new("Lower");
	gtk_grid_attach(GTK_GRID(grid), label, 3, row, 1, 1);
	row++;

	label = gtk_label_new("X");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 1, 1);

	boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT] = gtk_spin_button_new_with_range(0.0, 180.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT]), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT]), 180.0);
	boneRotationLimitSpinHandler[X_AXIS][UPPER_LIMIT] =
			g_signal_connect(G_OBJECT(boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT]), "value-changed", G_CALLBACK(updateBoneRotationLimits), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneRotationLimitSpinButton[X_AXIS][UPPER_LIMIT], 2, row, 1, 1);

	boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT] = gtk_spin_button_new_with_range(-180.0, 0.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT]), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT]), -180.0);
	boneRotationLimitSpinHandler[X_AXIS][LOWER_LIMIT] =
			g_signal_connect(G_OBJECT(boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT]), "value-changed", G_CALLBACK(updateBoneRotationLimits), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneRotationLimitSpinButton[X_AXIS][LOWER_LIMIT], 3, row, 1, 1);
	row++;

	label = gtk_label_new("Y");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 1, 1);

	boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT] = gtk_spin_button_new_with_range(0.0, 180.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT]), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT]), 180.0);
	boneRotationLimitSpinHandler[Y_AXIS][UPPER_LIMIT] =
			g_signal_connect(G_OBJECT(boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT]), "value-changed", G_CALLBACK(updateBoneRotationLimits), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneRotationLimitSpinButton[Y_AXIS][UPPER_LIMIT], 2, row, 1, 1);

	boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT] = gtk_spin_button_new_with_range(-180.0, 0.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT]), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT]), -180.0);
	boneRotationLimitSpinHandler[Y_AXIS][LOWER_LIMIT] =
			g_signal_connect(G_OBJECT(boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT]), "value-changed", G_CALLBACK(updateBoneRotationLimits), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneRotationLimitSpinButton[Y_AXIS][LOWER_LIMIT], 3, row, 1, 1);
	row++;

	label = gtk_label_new("Z");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 1, 1);

	boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT] = gtk_spin_button_new_with_range(0.0, 180.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT]), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT]), 180.0);
	boneRotationLimitSpinHandler[Z_AXIS][UPPER_LIMIT] =
			g_signal_connect(G_OBJECT(boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT]), "value-changed", G_CALLBACK(updateBoneRotationLimits), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneRotationLimitSpinButton[Z_AXIS][UPPER_LIMIT], 2, row, 1, 1);

	boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT] = gtk_spin_button_new_with_range(-180.0, 0.0, 0.5);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT]), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT]), -180.0);
	boneRotationLimitSpinHandler[Z_AXIS][LOWER_LIMIT] =
			g_signal_connect(G_OBJECT(boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT]), "value-changed", G_CALLBACK(updateBoneRotationLimits), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneRotationLimitSpinButton[Z_AXIS][LOWER_LIMIT], 3, row, 1, 1);
	row++;

	label = gtk_label_new(" ");
	gtk_grid_attach(GTK_GRID(grid), label, 1, row, 3, 1);
	row++;

	boneStore = gtk_tree_store_new(1, G_TYPE_STRING);
	boneView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(boneStore));

	GtkCellRenderer * renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn * column = gtk_tree_view_column_new_with_attributes("Bone structure", renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(boneView), column);

	boneSelect = gtk_tree_view_get_selection(GTK_TREE_VIEW(boneView));
	g_signal_connect(G_OBJECT(boneSelect), "changed", G_CALLBACK(selectBone), NULL);
	gtk_grid_attach(GTK_GRID(grid), boneView, 1, row, 3, 1);
	row++;

	gtk_container_add(GTK_CONTAINER(window), grid);

	return window;
}

GtkWidget * createAnimationWindow() {
	GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL), * grid = gtk_grid_new(), * label, * button;
	gtk_window_set_title(GTK_WINDOW(window), "Animation");
	gtk_window_set_resizable(GTK_WINDOW(window), false);
	g_signal_connect(window, "destroy", G_CALLBACK(switchMode), &trueBool);

	int col = 1;

	button = gtk_button_new_with_label("Add animation");
	g_signal_connect(button, "clicked", G_CALLBACK(addAnimation), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, col, 1, 3, 1);
	col += 3;

	animationSelectSpinButton = gtk_spin_button_new_with_range(0, 0, 1);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(animationSelectSpinButton), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(animationSelectSpinButton), 60);
	g_signal_connect(G_OBJECT(animationSelectSpinButton), "value-changed", G_CALLBACK(selectAnimation), NULL);
	gtk_grid_attach(GTK_GRID(grid), animationSelectSpinButton, col, 1, 2, 1);
	col += 2;

	label = gtk_label_new("Animation name: ");
	gtk_grid_attach(GTK_GRID(grid), label, col, 1, 3, 1);
	col += 3;

	animationNameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(animationNameEntry), animations[currentAnimation].name.c_str());
	gtk_entry_set_width_chars(GTK_ENTRY(animationNameEntry), 12);
	gtk_entry_set_max_length(GTK_ENTRY(animationNameEntry), 16);
	g_signal_connect(G_OBJECT(animationNameEntry), "activate", G_CALLBACK(renameAnimation), NULL);
	gtk_grid_attach(GTK_GRID(grid), animationNameEntry, col, 1, 2, 1);
	col += 2;

	label = gtk_label_new(" ");
	gtk_grid_attach(GTK_GRID(grid), label, col, 1, 1, 1);
	col++;

	label = gtk_label_new("Animation length: ");
	gtk_grid_attach(GTK_GRID(grid), label, col, 1, 3, 1);
	col += 3;

	animationLengthSpinButton = gtk_spin_button_new_with_range(1, 999, 1);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(animationLengthSpinButton), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(animationLengthSpinButton), 60);
	g_signal_connect(G_OBJECT(animationLengthSpinButton), "value-changed", G_CALLBACK(updateAnimationLength), NULL);
	gtk_grid_attach(GTK_GRID(grid), animationLengthSpinButton, col, 1, 2, 1);
	col += 2;

	label = gtk_label_new(" Timeline jump: ");
	gtk_grid_attach(GTK_GRID(grid), label, col, 1, 3, 1);
	col += 3;

	timelineJumpEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(timelineJumpEntry), "0");
	gtk_entry_set_width_chars(GTK_ENTRY(timelineJumpEntry), 4);
	gtk_entry_set_max_length(GTK_ENTRY(timelineJumpEntry), 3);
	g_signal_connect(G_OBJECT(timelineJumpEntry), "activate", G_CALLBACK(timelineJump), NULL);
	gtk_grid_attach(GTK_GRID(grid), timelineJumpEntry, col, 1, 2, 1);
	col += 2;

	button = gtk_button_new_with_label("Set keyframe");
	g_signal_connect(button, "clicked", G_CALLBACK(setKeyframeCallback), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, col, 1, 3, 1);
	col += 3;

	autoKeyToggleButton = gtk_toggle_button_new_with_label("AutoKey");
	autoKeyToggleHandler = g_signal_connect(autoKeyToggleButton, "toggled", G_CALLBACK(toggleAutoKey), NULL);
	gtk_grid_attach(GTK_GRID(grid), autoKeyToggleButton, col, 1, 3, 1);
	col += 3;

	playAnimationToggleButton = gtk_toggle_button_new_with_label("Play Animation");
	playAnimationToggleHandler = g_signal_connect(playAnimationToggleButton, "toggled", G_CALLBACK(togglePlayAnimation), NULL);
	gtk_grid_attach(GTK_GRID(grid), playAnimationToggleButton, col, 1, 3, 1);
	col += 3;

	button = gtk_button_new_with_label("Delete animation");
	g_signal_connect(button, "clicked", G_CALLBACK(deleteAnimation), NULL);
	gtk_grid_attach(GTK_GRID(grid), button, col, 1, 3, 1);
	col += 3;

	timeline = gtk_hscale_new_with_range(1, 60, 1);
	g_signal_connect(G_OBJECT(timeline), "value-changed", G_CALLBACK(setCurrentFrameFromScale), NULL);
	gtk_grid_attach(GTK_GRID(grid), timeline, 1, 2, col, 1);

	gtk_container_add(GTK_CONTAINER(window), grid);

	return window;
}

int main(int argc, char *argv[]) {
	gtk_init(&argc, &argv);

	createGlWindow();
	animations.push_back((animationDetail){"animation0", 60});

	gtk_widget_show_all(createToolsWindow());
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewToggleButton[FRONT]), 1);

	boneWindow = createBoneWindow();
	animationWindow = createAnimationWindow();
	gtk_widget_show_all(boneWindow);

	gtk_main();

	destroyGlWindow();

	return 0;
}
