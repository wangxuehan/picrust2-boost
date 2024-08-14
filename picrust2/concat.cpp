#include <string>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <zlib.h>
#define SPLIT ","
#define DELIM "\t"
#define NEWLINE "\n"
#define BUF_SIZE 1024 * 1024

int getIndex(std::set<std::string> &h, std::string c, int &index)
{
    index = 0;
    std::set<std::string>::iterator it;
    for (it = h.begin(); it != h.end(); it++)
    {
        // printf("%s %s %d\n", (*it).c_str(), c.c_str(), index);
        if (*it == c)
        {
            // printf("%s %s %d\n", (*it).c_str(), c.c_str(), index);
            return 1;
        }
        index++;
    }
    return 0;
}

int read_header(const char *file, std::set<std::string> &header)
{
    FILE *ifile = fopen(file, "r");
    if (ifile == NULL)
    {
        perror(file);
        exit(1);
    }
    char buf[BUF_SIZE];
    // int col = 0;
    if (fgets(buf, BUF_SIZE, ifile) != NULL)
    {
        char *token = strtok(buf, DELIM);
        // 跳过index列
        token = strtok(NULL, DELIM);
        while (token != NULL)
        {
            header.insert(token);
            token = strtok(NULL, DELIM);
            // col++;
        }
    }
    fclose(ifile);
    return 0;
}

int loc_file(const char *file, std::set<std::string> &header, std::map<int, std::string> &file_loc)
{
    FILE *ifile = fopen(file, "r");
    if (ifile == NULL)
    {
        perror(file);
        exit(1);
    }
    char buf[BUF_SIZE];
    int col = 0;
    int loc = 0;
    if (fgets(buf, BUF_SIZE, ifile) != NULL)
    {
        char *token = strtok(buf, DELIM);
        // 跳过index列
        token = strtok(NULL, DELIM);
        col += 2;
        getIndex(header, token, loc);
        // printf("loc: %d token: %s\n", loc, token);
        // while (token != NULL)
        // {
        //     header.insert(token);
        //     token = strtok(NULL, DELIM);
        //     col++;
        // }
    }
    file_loc[loc] = file;
    // printf("loc: %d file_loc[loc]: %s\n", loc, file_loc[loc].c_str());
    fclose(ifile);
    return 0;
}

bool cmp(const std::string &a, const std::string &b)
{
    return a < b;
}

std::unordered_map<std::string, std::string> final_out;
std::vector<std::string> index_order;

int read_write(char *file, std::map<int, std::string> &file_loc)
{
    int file_num = 0;
    char *zero_cell;
    for (auto it : file_loc)
    {
        FILE *ifile = fopen(it.second.c_str(), "r");
        if (ifile == NULL)
        {
            perror(it.second.c_str());
            exit(1);
        }
        char buf[BUF_SIZE];
        int line = 0;
        while (fgets(buf, BUF_SIZE, ifile) != NULL)
        {
            char *strip = strtok(buf, NEWLINE);
            char *buf_tmp = strdup(buf);
            // char *index = strpbrk(buf, SPLIT);
            // if (index && *index != '\0')
            //     *index++ = '\0';
            char *index = strtok(buf_tmp, DELIM);
            // printf("index: %s strip: %s\n", index, strip);
            if (file_num == 0)
            {
                final_out[index] = strip;
                if (line == 0)
                    zero_cell = strdup(index);
                else
                    index_order.emplace_back(index);
                // printf("index: %s strip: %s zero_cell: %s\n", index, final_out[index].c_str(), zero_cell);
            }
            else
            {
                char *content = strpbrk(strip, DELIM);
                // printf("index: %s content: %s\n", index, content);
                final_out[index].append(content);
            }
            line++;
        }
        fclose(ifile);
        file_num++;
    }
    sort(index_order.begin(), index_order.end(), cmp);

    size_t len = strlen(file);
    char *sub = file + len - 3;
    if (strcmp(sub, ".gz") == 0)
    {
        gzFile gz = gzopen(file, "wb");
        if (!gz)
        {
            perror("gzopen error");
            exit(1);
        }
        // // printf("%s\n", final_out[zero_cell].c_str());
        // gzprintf(gz, "%s\n", final_out[zero_cell].c_str());
        // for (auto it : index_order)
        // {
        //     // printf("sequence: %s %s\n", it, final_out[it].c_str());
        //     gzprintf(gz, "%s\n", final_out[it].c_str());
        // }
        // // printf("%s\n", final_out[zero_cell].c_str());

        char *buf = (char *)malloc(sizeof(char) * (final_out[zero_cell].size() + 1)); // 分配一个缓冲区，大小为字符串长度加1
        strcpy(buf, final_out[zero_cell].c_str());                                    // 将字符串复制到缓冲区
        buf[final_out[zero_cell].size()] = '\n';                                      // 在缓冲区末尾添加换行符
        gzwrite(gz, buf, final_out[zero_cell].size() + 1);                            // 向gzip文件写入缓冲区内容
        free(buf);                                                                    // 释放缓冲区
        for (auto it : index_order)
        {
            // printf("sequence: %s %s\n", it, final_out[it].c_str());
            char *buf = (char *)malloc(sizeof(char) * (final_out[it].size() + 1)); // 同上
            strcpy(buf, final_out[it].c_str());
            buf[final_out[it].size()] = '\n';
            gzwrite(gz, buf, final_out[it].size() + 1);
            free(buf);
        }

        gzclose(gz);
    }
    else
    {
        FILE *ofile = fopen(file, "w");
        if (ofile == NULL)
        {
            perror(file);
            exit(1);
        }
        fprintf(ofile, "%s\n", final_out[zero_cell].c_str());
        for (auto it : index_order)
        {
            // printf("sequence: %s %s\n", it, final_out[it].c_str());
            fprintf(ofile, "%s\n", final_out[it].c_str());
        }
        fclose(ofile);
    }
    return 0;
}

// void split(std::string str, const const char split, std::vector<std::string> &res)
// {
//     std::istringstream iss(str);       // 输入流
//     std::string token;                 // 接收缓冲区
//     while (getline(iss, token, split)) // 以split为分隔符
//     {
//         res.push_back(token);
//     }
// }

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage:\n\t%s file1,file2 outfile\n\nAuthor: wangxuehan\n", argv[0]);
        exit(1);
    }

    // read header
    std::set<std::string> header;
    std::map<int, std::string> file_loc;
    std::string file_str = argv[1];
    std::istringstream iss(file_str);
    // char *token = strtok(file_str, SPLIT);
    std::string token;
    std::vector<std::string> file_list;
    const char split = ',';
    while (getline(iss, token, split))
    {
        read_header(token.c_str(), header);
        // printf("file: %s\n", token.c_str());
        // printf("argv[1]: %s\n", argv[1]);
        file_list.emplace_back(token);
        // token = strtok(NULL, SPLIT);
        // printf("file: %s file_str: %s\n", token, file_str);
    }

    for (auto file : file_list)
    {
        // printf("file: %s\n", file);
        loc_file(file.c_str(), header, file_loc);
    }

    read_write(argv[2], file_loc);
}
