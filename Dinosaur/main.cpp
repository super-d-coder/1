#include<graphics.h>
#include<conio.h>
#include<cstdio>
#include<math.h>
#include<ctime>
#include <unordered_map>
#include <string>
#include <mmsystem.h> // 添加多媒体支持
#pragma comment(lib, "winmm.lib") // 链接 winmm 库
using namespace std;

/// 保存最高分到文件
void SaveHighScore(int score) {
	FILE* file;
	fopen_s(&file, "highscore.dat", "wb"); // 使用安全的fopen_s
	if (file) {
		fwrite(&score, sizeof(int), 1, file);
		fclose(file);
	}
}

// 从文件读取最高分
int LoadHighScore() {
	int score = 0;
	FILE* file;
	fopen_s(&file, "highscore.dat", "rb"); // 使用安全的fopen_s
	if (file) {
		fread(&score, sizeof(int), 1, file);
		fclose(file);
	}
	return score;
}

// 矩形结构体，用于碰撞检测
struct Rect {
	double x, y, w, h;
	bool CollidesWith(const Rect& other) const {
		return x < other.x + other.w &&
			x + w > other.x &&
			y < other.y + other.h &&
			y + h > other.y;
	}
};

// 障碍物类型
enum ObstacleType { NORMAL, WIDE, TALL };

// 资源管理类
class ResourceManager {
private:
	unordered_map<string, IMAGE> images;
public:
	void Load(const string& name, const TCHAR* path) {
		loadimage(&images[name], path);
	}

	IMAGE* Get(const string& name) {
		if (images.find(name) != images.end()) {
			return &images[name];
		}
		return nullptr;
	}
};

// 小恐龙
class Dinosaur {
private:
	Rect rect; // 位置和大小
	double dino_vx; // 水平速度
	double dino_vy; // 竖直速度
	double gravity; // 重力加速度
	int ground_flag; // 0:空中, 1:地面可跳一次, 2:地面可跳两次
	int animState; // 0:奔跑, 1:跳跃, 2:下蹲
	int animFrame; // 当前动画帧

public:
	friend class Game;
	// 初始化恐龙
	Dinosaur() : rect{ 1000 / 20.0, (400 - 46) / 4.0 - 16, 14, 16 }, dino_vx(0), dino_vy(0), gravity(0.25), ground_flag(1), animState(0), animFrame(0) {}

	void Update() {
		// 更新位置
		dino_vy += gravity;
		rect.y += dino_vy;
		rect.x += dino_vx;

		// 更新动画状态
		if (ground_flag > 0) {
			animState = 0; // 奔跑
			animFrame = (animFrame + 1) % 9; // 更新动画帧
		}
		else {
			animState = 1; // 跳跃
		}
	}

	void Jump() {
		if (ground_flag > 0) {
			dino_vy = -5;
			ground_flag--;
		}
	}

	void Duck() {
		dino_vy = 5;
		dino_vx = 0;
	}

	void MoveRight() {
		dino_vx = 1;
	}

	void MoveLeft() {
		dino_vx = -1;
	}

	void Dash() {
		if (ground_flag >= 0) {
			rect.x += 10;
		}
	}

	void Reset() {
		rect = { 1000 / 20.0, (400 - 46) / 4.0 - 16, 14, 16 };
		dino_vx = 0;
		dino_vy = 0;
		ground_flag = 1;
	}

	const Rect& GetRect() const { return rect; }
};

// 障碍物
class Obstacle {
private:
	Rect rect;
	int flag; // 用于标识障碍物
	ObstacleType type; // 障碍物类型

public:
	friend class Game;
	// 添加默认构造函数
	Obstacle() : rect{ 0,0,0,0 }, flag(0), type(NORMAL) {}

	Obstacle(double x, double y, double w, double h, int f, ObstacleType t)
		: rect{ x, y, w, h }, flag(f), type(t) {}

	void Update(double vx) {
		rect.x += vx;
	}

	void Reset(double x, double y, ObstacleType t) {
		rect.x = x;
		rect.y = y;
		type = t;

		// 根据类型设置不同尺寸
		switch (type) {
		case NORMAL:
			rect.w = 16;
			rect.h = 21;
			break;
		case WIDE:
			rect.w = 32; // 宽度是普通的两倍
			rect.h = 21;
			break;
		case TALL:
			rect.w = 16;
			rect.h = 42; // 高度是普通的两倍
			break;
		}
	}

	const Rect& GetRect() const { return rect; }
	ObstacleType GetType() const { return type; }
};

// 游戏状态
enum GameState { INTRO, MAIN_MENU, PLAYING, TUTORIAL, GAME_OVER };

// 主游戏
class Game {
private:
	// 游戏设置
	double width, height, bottom;
	double rec_distance, rec_vx;
	double cloud_x, cloud_x2;
	int score, tmpscore1, tmpscore2;
	int skill1, skill2; // 是否获得技能1与技能2
	int skill1_flag;    // 技能1是否可用
	int tutorialStep;   // 教程步骤
	clock_t introStartTime; // 记录过场动画开始时间
	GameState lastState; // 记录上一个状态，用于检测状态变化
	bool bgmPlaying;    // 标记是否有音乐正在播放
	bool nightmareMode;	//是否开启噩梦模式
	int highScore; // 最高分记录

	// 游戏对象
	Dinosaur dino;
	Obstacle obs1, obs2; // 使用默认构造函数初始化

	// 资源
	ResourceManager resources;

	// 游戏状态
	GameState state;

	// 初始化游戏设置
	void init() {
		// 加载图片
		resources.Load("bg", _T("./src/background.png"));
		resources.Load("logo1", _T("./src/logo1.png"));
		resources.Load("logo2", _T("./src/logo2.png"));
		resources.Load("dino21", _T("./src/move2_1.png"));
		resources.Load("dino22", _T("./src/move2_2.png"));
		resources.Load("dino31", _T("./src/move3_1.png"));
		resources.Load("dino32", _T("./src/move3_2.png"));
		resources.Load("dino41", _T("./src/move4_1.png"));
		resources.Load("dino42", _T("./src/move4_2.png"));
		resources.Load("fly1", _T("./src/fly_1.png"));
		resources.Load("fly2", _T("./src/fly_2.png"));
		resources.Load("ground", _T("./src/1.png"));
		resources.Load("Tree", _T("./src/Tree.png"));
		resources.Load("cloud", _T("./src/cloud.png"));

		// 初始化游戏参数
		width = 1000;
		height = 400;
		bottom = (height - 46) / 4;
		rec_distance = 100;
		rec_vx = -2.0;
		cloud_x = 220;
		cloud_x2 = 55;
		score = 0;
		tmpscore1 = 10; // 闪现技能解锁分数
		tmpscore2 = 25; // 二段跳技能解锁分数
		skill1 = 0;
		skill2 = 0;
		skill1_flag = 1;
		tutorialStep = 0;
		bgmPlaying = false;
		nightmareMode = false;
		highScore = LoadHighScore();//加载最高分

		// 初始化恐龙和障碍物（初始都是普通类型）
		dino.Reset();
		obs1 = Obstacle(width / 4 - 25, bottom - 21, 16, 21, 0, NORMAL);
		obs2 = Obstacle(obs1.GetRect().x + rec_distance, bottom - 21, 16, 21, 0, NORMAL);

		// 初始化游戏状态为过场动画
		state = INTRO;
		lastState = INTRO;
		introStartTime = clock();
	}

	// 播放MP3音乐
	void PlayMP3(const TCHAR* filename, bool loop = false) {
		// 关闭当前正在播放的音乐
		StopBGM();

		TCHAR cmd[256];
		// 打开音乐文件
		swprintf_s(cmd, _T("open \"%s\" type mpegvideo alias bgm"), filename);
		mciSendString(cmd, NULL, 0, NULL);

		// 播放音乐
		if (loop) {
			mciSendString(_T("play bgm repeat"), NULL, 0, NULL);
		}
		else {
			mciSendString(_T("play bgm"), NULL, 0, NULL);
		}

		bgmPlaying = true;
	}

	// 停止播放音乐
	void StopBGM() {
		if (bgmPlaying) {
			mciSendString(_T("stop bgm"), NULL, 0, NULL);
			mciSendString(_T("close bgm"), NULL, 0, NULL);
			bgmPlaying = false;
		}
	}

	// 渲染过场动画
	void render_intro() {
		putimage(0, 0, resources.Get("bg"));

		// 只显示游戏标题和logo
		settextstyle(15, 0, _T("Consolas"));
		settextcolor(RGB(253, 138, 101));

		TCHAR Intro[30];
		wcscpy_s(Intro, _T("New Dinosaur"));
		int Intro_wid = width / 8 - textwidth(Intro) / 2;
		int Intro_hei = height / 8 - textheight(Intro);
		outtextxy(Intro_wid + 10, Intro_hei, Intro);

		// 绘制logo恐龙
		PutPng(Intro_wid - dino.GetRect().w * 2 + 10, Intro_hei - 1,
			resources.Get("logo1"), resources.Get("logo2"));

		// 绘制地板
		for (int i = 0; i < width / 46; i++) {
			putimage(i * 46, bottom, resources.Get("ground"));
		}

		// 1.5秒后自动切换到主菜单
		if (clock() - introStartTime > 1500) {
			state = MAIN_MENU;
		}
	}

	// 渲染主菜单
	void render_menu() {
		putimage(0, 0, resources.Get("bg"));

		// 绘制云朵
		putimage(cloud_x, 10, resources.Get("cloud"));
		putimage(cloud_x2, 15, resources.Get("cloud"));

		// 绘制地面上的恐龙（游戏角色）
		PutPng(dino.GetRect().x, dino.GetRect().y,
			resources.Get("logo1"), resources.Get("logo2"));

		// 绘制地板
		for (int i = 0; i < width / 46; i++) {
			putimage(i * 46, bottom, resources.Get("ground"));
		}

		settextcolor(RGB(0, 0, 0));
		settextstyle(8, 0, _T("宋体"));

		// 绘制提示字体
		TCHAR Intro[30];
		wcscpy_s(Intro, _T("开始 -   空格"));
		int Intro_wid = width / 8 - textwidth(Intro) / 2;
		int Intro_hei = height / 8 - textheight(Intro) * 2;
		outtextxy(Intro_wid, Intro_hei, Intro);

		wcscpy_s(Intro, _T("教程 - press A"));
		Intro_hei = Intro_hei + textheight(Intro);
		outtextxy(Intro_wid, Intro_hei + 5, Intro);

		wcscpy_s(Intro, _T("退出 - press B"));
		outtextxy(Intro_wid, Intro_hei + textheight(Intro) + 10, Intro);

		// 在左上角显示最高分
		settextcolor(RGB(255, 255, 255)); // 白色文字
		settextstyle(14, 0, _T("Consolas"));
		TCHAR highScoreText[50];
		swprintf_s(highScoreText, _T("最高分: %d"), highScore);
		outtextxy(10, 10, highScoreText);
	}

	// 主菜单互动功能
	void handle_menu_input() {
		if (_kbhit()) {
			char input = _getch();
			switch (input) {
			case ' ': // 空格开始游戏
				state = PLAYING;
				break;
			case 'B': case 'b': // 退出游戏
				StopBGM(); // 停止音乐
				closegraph();
				exit(0);
			case 'A': case 'a': // 进入教程
				state = TUTORIAL;
				tutorialStep = 0;
				break;
			}
		}
	}

	// 游戏主体
	void update_game() {
		handle_game_input();

		// 更新位置
		dino.Update();
		obs1.Update(rec_vx);
		obs2.Update(rec_vx);

		// 云朵移动
		cloud_x -= 0.5;
		cloud_x2 -= 0.5;

		// 地面检测
		if (dino.GetRect().y >= bottom - dino.GetRect().h) {
			dino.rect.y = bottom - dino.GetRect().h;
			dino.dino_vy = 0;
			dino.ground_flag = skill2 ? 2 : 1;
			skill1_flag = 1;
		}

		// 碰撞检测
		if (dino.GetRect().CollidesWith(obs1.GetRect()) ||
			dino.GetRect().CollidesWith(obs2.GetRect()) ||
			dino.GetRect().x <= 0 ||
			dino.GetRect().x + dino.GetRect().w >= width / 4) {
			state = GAME_OVER;
			StopBGM(); // 停止音乐
		}

		// 障碍物循环
		recircle();

		// 云朵循环
		if (cloud_x <= 0) cloud_x = width / 4 + 27;
		if (cloud_x2 <= 0) cloud_x2 = width / 4 + 63;

		// 技能获得判定
		check_skills();
	}

	// 障碍物循环
	void recircle() {
		// 障碍物跑到尽头
		if (obs1.GetRect().x <= 0) {
			obs1.flag++;

			// 根据分数决定障碍物类型
			ObstacleType newType = NORMAL;
			if (score >= tmpscore1 && score < tmpscore2) {
				newType = NORMAL; // 10分后第一个障碍物保持普通
			}
			else if (score >= tmpscore2) {
				newType = WIDE; // 25分后第一个障碍物变为宽障碍物
			}

			obs1.Reset(width / 4, bottom - 21, newType);

			if (obs2.GetRect().x + obs1.GetRect().w >= width / 4) {
				obs1.Reset(obs2.GetRect().x + (rand() % 3 + 2) * 50, bottom - 21, newType);
			}
			score++;
			if (score >= 10 && !nightmareMode) {
				rec_vx -= 0.1;
			}
		}
		else if (obs2.GetRect().x <= 0) {
			obs2.flag++;

			// 根据分数决定障碍物类型
			ObstacleType newType = NORMAL;
			if (score >= tmpscore1 && score < tmpscore2) {
				newType = WIDE; // 10分后第二个障碍物变为宽障碍物
			}
			else if (score >= tmpscore2) {
				newType = TALL; // 25分后第二个障碍物变为高障碍物
			}

			rec_distance = rand() % 5 * 16 + 16; // 随机距离
			if (obs1.flag > obs2.flag) rec_distance = width / 4 - obs1.GetRect().x;
			obs2.Reset(width / 4 + rec_distance + rand() % 5 * 16, bottom - 21, newType);
			score++;
		}
	}

	// 处理游戏输入
	void handle_game_input() {
		if (_kbhit()) {
			char input = _getch();
			if ((int)(input) == -32) {
				input = _getch();
				// 方向键 上 控制跳跃
				if ((int)input == 72 && dino.ground_flag >= 1) {
					dino.Jump();
					skill1_flag = 0;
				}
				// 方向键 下 控制下落
				if ((int)input == 80) {
					dino.Duck();
				}
				// 方向键 右 控制加速
				if ((int)input == 77) {
					dino.MoveRight();
					if (score >= tmpscore1 && dino.ground_flag >= 0 && skill1_flag == 0) {
						dino.Dash();
						skill1_flag = 1;
					}
				}
				// 方向键 左 控制减速
				if ((int)input == 75) {
					dino.MoveLeft();
				}
			}
		}
	}

	void check_skills() {
		if (score >= tmpscore1 && skill1 == 0) {
			skill1 = 1;
			settextstyle(12, 0, _T("宋体"));

			// 两行文本
			TCHAR line1[] = _T("获得新技能：空中向右可瞬移！");
			TCHAR line2[] = _T("注意：即将出现宽障碍物！");

			// 计算居中位置
			int textWidth = max(textwidth(line1), textwidth(line2));
			int x = width / 8 - textWidth / 2;
			int y = height / 6; // 向下移动

			outtextxy(x, y, line1);
			outtextxy(x, y + textheight(line1) + 5, line2);

			FlushBatchDraw();
			Sleep(2000);
		}

		if (score >= tmpscore2 && skill2 == 0) {
			skill2 = 1;
			settextstyle(12, 0, _T("宋体"));

			// 两行文本
			TCHAR line1[] = _T("获得新技能：空中可二段跳！");
			TCHAR line2[] = _T("注意：即将出现高障碍物！");

			// 计算居中位置
			int textWidth = max(textwidth(line1), textwidth(line2));
			int x = width / 8 - textWidth / 2;
			int y = height / 6; // 向下移动

			outtextxy(x, y, line1);
			outtextxy(x, y + textheight(line1) + 5, line2);

			FlushBatchDraw();
			Sleep(2000);
		}

		if (score >= 50 && !nightmareMode) {
			nightmareMode = true;
			settextstyle(12, 0, _T("宋体"));

			// 两行文本
			TCHAR line1[] = _T("警告：游戏即将开启噩梦模式！");
			TCHAR line2[] = _T("障碍物速度大幅提升！");

			// 计算居中位置
			int textWidth = max(textwidth(line1), textwidth(line2));
			int x = width / 8 - textWidth / 2;
			int y = height / 6; // 向下移动

			outtextxy(x, y, line1);
			outtextxy(x, y + textheight(line1) + 5, line2);

			FlushBatchDraw();
			Sleep(2000);

			// 关闭加速机制，设置高速
			rec_vx = -5.0;
		}
	}

	// 渲染游戏画面
	void render_game() {
		// 绘制背景
		putimage(0, 0, resources.Get("bg"));
		putimage(cloud_x, 10, resources.Get("cloud"));
		putimage(cloud_x2, 15, resources.Get("cloud"));

		// 绘制恐龙
		IMAGE* mask = nullptr;
		IMAGE* image = nullptr;

		if (dino.animState == 0) { // 奔跑状态
			if (dino.animFrame <= 3) {
				mask = resources.Get("dino21");
				image = resources.Get("dino22");
			}
			else if (dino.animFrame <= 5) {
				mask = resources.Get("dino31");
				image = resources.Get("dino32");
			}
			else {
				mask = resources.Get("dino41");
				image = resources.Get("dino42");
			}
		}
		else { // 跳跃状态
			mask = resources.Get("fly1");
			image = resources.Get("fly2");
		}

		if (mask && image) {
			PutPng(dino.GetRect().x, dino.GetRect().y, mask, image);
		}

		// 绘制地板
		for (int i = 0; i < width / 46; i++) {
			putimage(i * 46, bottom, resources.Get("ground"));
		}

		// 绘制障碍物
		draw_obstacle(obs1);
		draw_obstacle(obs2);

		// 绘制分数
		TCHAR s[20];
		swprintf_s(s, _T("%d"), score);
		settextstyle(14, 0, _T("Consolas"));
		outtextxy(50 / 4, 30 / 4, s);
	}

	// 绘制障碍物（根据类型）
	void draw_obstacle(const Obstacle& obs) {
		IMAGE* tree = resources.Get("Tree");
		Rect rect = obs.GetRect();

		switch (obs.GetType()) {
		case NORMAL:
			putimage(rect.x, rect.y, tree);
			break;

		case WIDE:
			// 宽障碍物：两个树并排
			putimage(rect.x, rect.y, tree);
			putimage(rect.x + 16, rect.y, tree); // 宽度为普通两倍
			break;

		case TALL:
			// 高障碍物：两个树叠放
			putimage(rect.x, rect.y, tree);
			putimage(rect.x, rect.y - 21, tree); // 高度为普通两倍
			break;
		}
	}

	// 处理教程
	void update_tutorial() {
		if (_kbhit()) {
			char input = _getch();
			if ((int)(input) == -32) {
				input = _getch();
				// 方向键上
				if ((int)input == 72 && dino.ground_flag >= 1) {
					dino.Jump();
					tutorialStep = max(tutorialStep, 1);
				}
				// 方向键下
				if ((int)input == 80) {
					dino.Duck();
					tutorialStep = max(tutorialStep, 2);
				}
				// 方向键右
				if ((int)input == 77) {
					dino.MoveRight();
					tutorialStep = max(tutorialStep, 3);
					if (dino.ground_flag >= 0) {
						tutorialStep = max(tutorialStep, 5);
					}
				}
				// 方向键左
				if ((int)input == 75) {
					dino.MoveLeft();
					tutorialStep = max(tutorialStep, 4);
				}
			}
			if (input == ' ') {
				state = PLAYING;
			}
		}

		// 更新位置
		dino.Update();
		obs1.Update(rec_vx);
		obs2.Update(rec_vx);

		// 地面检测
		if (dino.GetRect().y >= bottom - dino.GetRect().h) {
			dino.rect.y = bottom - dino.GetRect().h;
			dino.dino_vy = 0;
			dino.ground_flag = 1;
		}
	}

	// 渲染教程
	void render_tutorial() {
		render_game();

		settextstyle(7, 0, _T("宋体"));
		TCHAR tmp[50];

		switch (tutorialStep) {
		case 0: wcscpy_s(tmp, _T("方向键上  控制跳跃")); break;
		case 1: wcscpy_s(tmp, _T("方向键下  控制下落")); break;
		case 2: wcscpy_s(tmp, _T("方向键右  控制水平向右加速")); break;
		case 3: wcscpy_s(tmp, _T("方向键左  控制水平向左加速")); break;
		case 4: wcscpy_s(tmp, _T("方向键右  技能:空中可瞬移")); break;
		default: wcscpy_s(tmp, _T("空格正式开始游戏")); break;
		}

		outtextxy(width / 8 - textwidth(tmp) / 2, height / 8, tmp);
	}

	// 渲染游戏结束
	void render_gameover() {
		// 更新最高分
		if (score > highScore) {
			highScore = score;
			SaveHighScore(highScore);
		}

		render_game();

		TCHAR tmp[20];
		swprintf_s(tmp, _T("Score:%d"), score);
		settextstyle(14, 0, _T("宋体"));
		outtextxy(width / 8 - textwidth(tmp) / 2, height / 8 - textheight(tmp), tmp);

		wcscpy_s(tmp, _T("空格-重新开始游戏"));
		settextstyle(7, 0, _T("宋体"));
		outtextxy(width / 8 - textwidth(tmp) / 2, height / 8 + 2 * textheight(tmp), tmp);

		if (_kbhit()) {
			char input = _getch();
			if (input == ' ') {
				init();
				state = PLAYING;
			}
		}
	}

	// 输出透明背景的图片
	void PutPng(int x, int y, IMAGE* mask, IMAGE* image) {
		putimage(x, y, mask, SRCAND);
		putimage(x, y, image, SRCPAINT);
	}

	// 处理音频播放
	void handle_audio() {
		// 如果状态发生变化，更新音频
		if (state != lastState) {
			lastState = state;

			switch (state) {
			case MAIN_MENU:
				// 播放主菜单音乐（循环）
				PlayMP3(_T("./src/bgm1.mp3"), true);
				break;
			case PLAYING:
			case TUTORIAL:
				// 播放游戏音乐（循环）
				PlayMP3(_T("./src/bgm2.mp3"), true);
				break;
			case GAME_OVER:
				// 停止所有音乐
				StopBGM();
				break;
			default:
				// 其他状态不播放音乐
				break;
			}
		}
	}

public:
	Game() : obs1(), obs2() { // 在初始化列表中初始化 obs1 和 obs2
		srand((int)time(0));
		init();
	}

	~Game() {
		StopBGM(); // 确保退出时停止音乐
	}

	void Run() {
		initgraph((int)width, (int)height);
		setaspectratio(4.0, 4.0);
		setbkcolor(RGB(177, 236, 240));
		settextcolor(RGB(0, 0, 0));

		while (true) {
			// 处理音频
			handle_audio();

			BeginBatchDraw();
			cleardevice();

			switch (state) {
			case INTRO:
				render_intro();
				break;
			case MAIN_MENU:
				render_menu();
				handle_menu_input();
				break;
			case PLAYING:
				update_game();
				render_game();
				break;
			case TUTORIAL:
				update_tutorial();
				render_tutorial();
				break;
			case GAME_OVER:
				render_gameover();
				break;
			}

			EndBatchDraw();
			Sleep(24);
		}
		StopBGM(); // 确保退出时停止音乐
		closegraph();
	}
};

int main() {
	Game game;
	game.Run();
	return 0;
}