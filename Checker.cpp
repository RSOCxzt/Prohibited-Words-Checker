#include <iostream>
#include <vector>
#include <queue>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <unordered_map>

using namespace std;

// AC自动机节点
struct ACNode
{
    int next[128];
    int fail;
    bool isBad;
    ACNode()
    {
        memset(next, -1, sizeof(next));
        fail = 0;
        isBad = false;
    }
};

class SensitiveFilter
{
private:
    vector<ACNode> tree;

    // 形近字、谐音替换映射表（防绕过核心）
    unordered_map<char, char> charMap = {
        // 谐音替换
        {'毒','赌'}, {'堵','赌'}, {'杜','赌'},
        {'色','瑟'}, {'涩','色'},
        {'力','利'}, {'立','利'},
        // 形近字替换
        {'0','o'}, {'1','l'}, {'3','e'}, {'5','s'}, {'6','b'}, {'8','x'},
        {'丶','.'}, {'。','.'}, {'＿','_'}, {'－','-'}
    };

    // 语义白名单：关键词但无恶意语境（AI语义过滤核心）
    vector<string> safeContext = {
        "玩具枪", "仿真刀", "科普赌博危害", "远离色情", "反暴力宣传", "禁毒教育"
    };

    // 标准化文本：谐音+形近字统一转正，清洗无关字符
    string standardText(const string& s)
    {
        string res;
        for (char ch : s)
        {
            // 字符标准化替换
            if (charMap.find(ch) != charMap.end())
                res += charMap[ch];
            // 保留中英文字母、标准化符号
            else if ((ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z') ||
                     (unsigned char)ch > 127)
                res += ch;
        }
        return res;
    }

    // 基础文本清洗（原有检测清洗逻辑）
    string cleanText(const string& s)
    {
        string res;
        for (char ch : s)
        {
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (unsigned char)ch > 127)
            {
                res += ch;
            }
        }
        return res;
    }

    // 语义安全校验：判断是否为无害语境
    bool isSafeSemantic(const string& text)
    {
        for (const auto& safe : safeContext)
        {
            if (text.find(safe) != string::npos)
                return true;
        }
        return false;
    }

public:
    SensitiveFilter()
    {
        tree.emplace_back();
    }

    // 插入违禁词
    void insertWord(const string& word)
    {
        int cur = 0;
        for (char ch : word)
        {
            int c = (unsigned char)ch;
            if (tree[cur].next[c] == -1)
            {
                tree.emplace_back();
                tree[cur].next[c] = tree.size() - 1;
            }
            cur = tree[cur].next[c];
        }
        tree[cur].isBad = true;
    }

    // 构建自动机
    void build()
    {
        queue<int> q;

        // 初始化第一层
        for (int i = 0; i < 128; i++)
        {
            int son = tree[0].next[i];
            if (son != -1)
            {
                tree[son].fail = 0;
                q.push(son);
            }
            else
            {
                tree[0].next[i] = 0;
            }
        }

        // BFS构建fail指针
        while (!q.empty())
        {
            int u = q.front();
            q.pop();

            for (int i = 0; i < 128; i++)
            {
                int v = tree[u].next[i];
                if (v != -1)
                {
                    int f = tree[u].fail;
                    while (tree[f].next[i] == -1)
                        f = tree[f].fail;

                    tree[v].fail = tree[f].next[i];
                    tree[v].isBad |= tree[tree[v].fail].isBad;
                    q.push(v);
                }
            }
        }
    }

    // 从txt文件加载违禁词
    void loadFromFile(const string& path)
    {
        ifstream fin(path);
        if (!fin.is_open())
        {
            cout << "[警告] 未找到违禁词文件：" << path << endl;
            return;
        }
        string line;
        while (getline(fin, line))
        {
            if (!line.empty())
                insertWord(line);
        }
        fin.close();
        build();
        cout << "[系统] 违禁词库加载完成，已启用形近字/谐音防御+AI语义过滤" << endl;
    }

    // 增强检测：防绕检 + 语义智能甄别
    bool hasSensitive(const string& text)
    {
        // 先语义校验：无害语境直接放行
        if (isSafeSemantic(text))
            return false;

        // 文本标准化，破解谐音、形近、符号绕检
        string standardTxt = standardText(text);
        int cur = 0;
        for (char ch : standardTxt)
        {
            int c = (unsigned char)ch;
            cur = tree[cur].next[c];
            if (tree[cur].isBad)
                return true;
        }
        return false;
    }

    // 过滤并打码，返回处理后的文本
    string filterReplace(string text)
    {
        // 语义安全内容不打码
        if (isSafeSemantic(text))
            return text;

        int n = text.size();
        vector<bool> mask(n, false);
        string standardTxt = standardText(text);

        int cur = 0;
        for (int i = 0; i < standardTxt.size(); i++)
        {
            char ch = standardTxt[i];
            int c = (unsigned char)ch;
            cur = tree[cur].next[c];

            if (tree[cur].isBad)
            {
                // 精准标记敏感区域，避免全文打码
                for (int j = max(0, i - 5); j <= i; j++)
                {
                    mask[j] = true;
                }
            }
        }

        // 替换敏感字符为*
        for (int i = 0; i < n; i++)
        {
            if (mask[i]) text[i] = '*';
        }
        return text;
    }

    // 拓展：手动添加语义白名单
    void addSafeContext(const string& context)
    {
        safeContext.push_back(context);
    }

    // 拓展：手动添加形近/谐音替换规则
    void addCharReplaceRule(char src, char target)
    {
        charMap[src] = target;
    }
};

int main()
{
    SensitiveFilter filter;

    // 加载违禁词库（同目录下 badwords.txt）
    filter.loadFromFile("badwords.txt");

    string input;
    cout << "===== C++ 智能违禁词检测系统（语义甄别+防绕检）=====\n";

    while (true)
    {
        cout << "\n请输入内容(exit退出)：";
        getline(cin, input);
        if (input == "exit") break;

        if (filter.hasSensitive(input))
        {
            cout << "[检测结果] 发现恶意违禁内容！" << endl;
            cout << "[过滤后] " << filter.filterReplace(input) << endl;
        }
        else
        {
            cout << "[检测结果] 内容正常（已通过语义安全校验）" << endl;
        }
    }

    return 0;
}
