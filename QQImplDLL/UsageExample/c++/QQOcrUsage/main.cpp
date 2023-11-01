#include <iostream>

#include "QQOcr.h"
#include "ocr_protobuf.pb.h"
#include "json.hpp"
#include "funclib.h"
#include "opencv2\opencv.hpp"

using namespace qqimpl;

void OnOcrReadPush(const char* pic_path, void* ocr_response_serialize, int serialize_size);

std::wstring Utf8ToUnicode(std::string utf8_str);
std::wstring ANSIToUnicode(std::string ansi_str);
std::string UnicodeToANSI(std::wstring utf16_str);
std::string UnicodeToUtf8(std::wstring utf16_str);
std::string Utf8ToANSI(std::string utf8_str);
std::string ANSIToUtf8(std::string ansi_str);


//返回0->不存在 1->是文件夹 2->是文件
int check_path_info(std::string path)
{
    struct _stat64i32 info;
    if (_stat(path.c_str(), &info) != 0) {  // does not exist
        return 0;
    }

    if (info.st_mode & S_IFDIR) // directory
        return 1;
    else
        return 2;
}

std::string myfullpath(std::string fpath) {
    char fullpath[_MAX_PATH];
    _fullpath(fullpath, fpath.c_str(), _MAX_PATH);
    return fullpath;
}

std::string getCurrentDir() {
    char fpath[MAX_PATH];
    DWORD dwRet = GetModuleFileNameA(NULL, fpath, MAX_PATH);
    std::string fdir = fpath;
    int index = fdir.rfind('\\');
    return fdir.substr(0, index + 1);
}

HANDLE g_hEvent = NULL;
nlohmann::json g_resultj;
int g_counter = 0;

std::string mytext(cv::Mat img, cv::Rect& rect) {

    //img = img.clone();
    int width = img.cols;
    int height = img.rows;
    char pic_path[1024];

    sprintf(pic_path, "D:\\mytext\\%di.png", g_counter++);
    //bool temp = cv::imwrite(pic_path, img);
    //assert(temp);

    const int border = 2;

    // int top, int bottom, int left, int right,
    cv::Scalar value = { 255, 255, 255 };
    cv::copyMakeBorder(img, img, //
        border, border, border, border, // 如果本来就存在边界内容，就是真实的延拓。
        cv::BORDER_CONSTANT, value);

    assert(width + 4 == img.cols);
    assert(height + 4 == img.rows);

    sprintf(pic_path, "D:\\mytext\\%d.png", g_counter++);
    g_counter = 0;
    auto temp = cv::imwrite(pic_path, img);
    assert(temp);

    while (true) {
        auto flag = qqocr::DoOCRTask(pic_path);
        assert(flag);
        //WaitForSingleObject(g_hEvent, INFINITE);
        auto code = WaitForSingleObject(g_hEvent, 1000 * 10);

        if (code == WAIT_OBJECT_0) {
            break;
        }
        if (code == WAIT_TIMEOUT) {
            printf("WAIT_TIMEOUT %s\n", pic_path);
        }
    }

    std::string result;
    if (!g_resultj.empty() && g_resultj["err_code"] == 0 && g_resultj["type"] == 0) {
        auto ocr_result = g_resultj["ocr_result"];
        for (int i = 0; i < ocr_result.size(); i++) {
            std::string utf8str = ocr_result[i]["utf8str"];
            if (!result.empty()) {
                result.append("\r\n");
            }
            result.append(utf8str);

            int lx = ocr_result[i]["rect"][0];
            int ly = ocr_result[i]["rect"][1];
            int rx = ocr_result[i]["rect"][2];
            int ry = ocr_result[i]["rect"][3];

            cv::Rect irect(lx - border, ly - border, rx - lx, ry - ly);
            if (i == 0) {
                rect = irect;
            }
            else {
                rect = rect | irect;
            }
        }
    }

    g_resultj.clear();
    if (result.empty()) {
        return " ";
    }
    return result;
}

void mytable(std::string& imgfile, std::string& jsonfile, std::string& ocrfile) {

    auto img = cv::imread(imgfile.c_str(), cv::IMREAD_COLOR);
    if (img.empty()) {
        return;
    }

    long length = 0;
    char* fdata = readfile(jsonfile.c_str(), length);
    auto json = nlohmann::json::parse(fdata);
    freefiledata(fdata);

    // margintop, marginright, marginbottom, marginleft
    auto margin = json["config"]["margin"];
    int margintop = margin[0];
    int marginright = margin[1];
    int marginbottom = margin[2];
    int marginleft = margin[3];

    // int top, int bottom, int left, int right,
    cv::Scalar value = { 255, 255, 255 };
    cv::copyMakeBorder(img, img, //
        margintop, marginbottom, marginleft, marginright, //
        cv::BORDER_CONSTANT, value);

    auto imgsize = json["imgsize"];
    int width = imgsize[0];
    int height = imgsize[1];
    assert(width == img.cols);
    assert(height == img.rows);

    auto& tables_cells = json["tables_cells"];
    for (auto& table : tables_cells) {
        for (auto& cell : table) {
            auto bbox = cell["bbox"];

            float xmin = bbox[0];
            float ymin = bbox[1];
            float xmax = bbox[2];
            float ymax = bbox[3];
            
            cell["bbox2"] = { int(round(xmin)), int(round(ymin)), int(round(xmax)), int(round(ymax)) };

            cv::Mat imgi(img, cv::Range(ymin, ymax + 1), cv::Range(xmin, xmax + 1));
            cv::Rect rect;
            cell["cell text"] = mytext(imgi, rect); // baidu_text(engineCache, img.crop((xmin, ymin, xmax, ymax)), debug = debug)
            cell["text rect"] = { rect.tl().x, rect.tl().y, rect.br().x, rect.br().y };
        }
    }
    writefile(ocrfile.c_str(), json.dump(2, ' ').c_str());
}

void mytable() {

    std::string indexfile = "E:\\kSource\\blog\\kvision\\ksample\\mytable\\index.json";
    long length = 0;
    char* fdata = readfile(indexfile.c_str(), length);
    auto index = nlohmann::json::parse(fdata);
    freefiledata(fdata);

    for (int i = 0; i < index.size(); i++) {
        nlohmann::json item = index[i];
        std::string imgfile = item["imgfile"];
        std::string jsonfile = item["jsonfile"];
        std::string ocrfile = item["ocrfile"];
        mytable(imgfile, jsonfile, ocrfile);
    }
}

// https://github.com/tronkko/dirent
/* This is your true main function */
int
main(int argc, char* argv[])
{
    //创建Event用于等待OCR任务返回
    g_hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (g_hEvent == NULL)
    {
        std::cout << "[!] CreateEventA Err: " << GetLastError() << "\n";
        return 1;
    }

    std::string ocr_path, usr_lib_path, pic_path;

    //std::cout << "[*] Enter WeChatOCR.exe Path:\n[>] ";
    //std::getline(std::cin, ocr_path);
    ocr_path = getCurrentDir() + "..\\..\\extracted\\WeChatOCR.exe";
    if (check_path_info(ocr_path) != 2) {
        ocr_path = getCurrentDir() + "..\\..\\..\\QQOcr\\bin\\extracted\\WeChatOCR.exe";
    }

    //std::cout << "[*] Enter mmmojo(_64).dll Path:\n[>] ";
    //std::getline(std::cin, usr_lib_path);
    usr_lib_path = getCurrentDir() + "..\\..\\"; // mmmojo_64.dll
    if (check_path_info(usr_lib_path + "mmmojo_64.dll") != 2) {
        usr_lib_path = getCurrentDir() + "..\\..\\..\\QQOcr\\bin\\"; // mmmojo_64.dll
    }

    //std::cout << "[*] Enter Pic Path to OCR (Default \'.\\test.jpg\'):\n[>] ";
    //std::getline(std::cin, pic_path);
    pic_path = "E:\\kSource\\pythonx\\decode\\wechatocr\\QQImpl\\test.jpg";
    if (pic_path.empty() || check_path_info(pic_path) != 2) 
        pic_path = ".\\test.jpg";
    if (pic_path.empty() || check_path_info(pic_path) != 2) 
        pic_path = "..\\..\\test.jpg";
    if (pic_path.empty() || check_path_info(pic_path) != 2) 
        pic_path = "..\\..\\..\\QQOcr\\bin\\test.jpg";

    bool cmdx = false;
    for (int i = 1; i < argc; i++) {
        if (check_path_info(argv[i]) == 2) {
            pic_path = ANSIToUtf8(argv[i]);
            cmdx = true;
        }
    }

    if (check_path_info(ocr_path) != 2)
    {
        std::cout << "[!] OCR Exe Path is not exist!\n";
        return 0;
    }

    if (check_path_info(usr_lib_path) != 1)
    {
        std::cout << "[!] OCR Usr Lib Path is not exist!\n";
        return 0;
    }

    ocr_path = myfullpath(ocr_path);
    usr_lib_path = myfullpath(usr_lib_path);
    pic_path = myfullpath(pic_path);

    //初始化OCR Manager
    qqocr::InitManager();
    //std::cout << "[+] InitOcrManager OK!\n";

    //设置WeChatOCR.exe所在目录
    if (!qqocr::SetOcrExePath(ocr_path.c_str()))
    {
        std::cout << "[!] SetOcrExePath Err: " << qqocr::GetLastErrStr() << "\n";
        return 1;
    }
    //std::cout << "[+] SetOcrExePath OK!\n";

    //设置mmmojo_64.dll所在目录
    if (!qqocr::SetOcrUsrLibPath(usr_lib_path.c_str()))
    {
        std::cout << "[!] SetOcrUsrLibPath Err: " << qqocr::GetLastErrStr() << "\n";
        return 1;
    }
    //std::cout << "[+] SetOcrUsrLibPath OK!\n";

    //设置接收OCR结果的回调函数
    qqocr::SetUsrReadPushCallback(OnOcrReadPush);
    //std::cout << "[+] SetOcrUsrReadPushCallback OK!\n";

    if (true) {
        mytable();
    }

    //发送OCR任务
    //std::cout << "[>] Press any key to Send OCR Task:";
    //getchar();
    auto flag = qqocr::DoOCRTask(pic_path.c_str());
    assert(flag);

    //等待OnOcrReadPush返回
    WaitForSingleObject(g_hEvent, INFINITE);
        
    //std::cout << "[>] Press any key to Shutdown OCR Env:";
    if (!cmdx) {
        std::cout << "Press any key to Shutdown...";
        getchar();
    }
    qqocr::UnInitManager();

    //std::cout << "[-] Shutdown WeChatOCR Env!\n";
    CloseHandle(g_hEvent);
    return 0;
}

/* Convert arguments to UTF-8 */
#ifdef _MSC_VER
int
wmain2(int argc, wchar_t* argv[])
{
    /* Select UTF-8 locale */
    setlocale(LC_ALL, ".utf8");
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    /* Allocate memory for multi-byte argv table */
    char** mbargv;
    mbargv = (char**)malloc(argc * sizeof(char*));
    if (!mbargv) {
        puts("Out of memory");
        exit(3);
    }

    /* Convert each argument in argv to UTF-8 */
    for (int i = 0; i < argc; i++) {
        size_t n;
        wcstombs_s(&n, NULL, 0, argv[i], 0);

        /* Allocate room for ith argument */
        mbargv[i] = (char*)malloc(n);
        if (!mbargv[i]) {
            puts("Out of memory");
            exit(3);
        }

        /* Convert ith argument to utf-8 */
        wcstombs_s(NULL, mbargv[i], n, argv[i], n);
    }

    /* Pass UTF-8 converted arguments to the main program */
    int errorcode = 0; // _main(argc, mbargv);

    /* Release UTF-8 arguments */
    for (int i = 0; i < argc; i++) {
        free(mbargv[i]);
    }

    /* Release the argument table */
    free(mbargv);
    return errorcode;
}
#else
int
main(int argc, char* argv[])
{
    return _main(argc, argv);
}
#endif

//以下为char与wchar的转换
std::wstring Utf8ToUnicode(std::string utf8_str)
{
    if (utf8_str.empty())
        return std::wstring();

    const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8_str.at(0), (int)utf8_str.size(), nullptr, 0);
    if (size_needed <= 0)
    {
        throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
    }

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8_str.at(0), (int)utf8_str.size(), &result.at(0), size_needed);
    return result;
}

//以下为char与wchar的转换
std::wstring ANSIToUnicode(std::string ansi_str)
{
    if (ansi_str.empty())
        return std::wstring();

    const auto size_needed = MultiByteToWideChar(CP_ACP, 0, &ansi_str.at(0), (int)ansi_str.size(), nullptr, 0);
    if (size_needed <= 0)
    {
        throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
    }

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, &ansi_str.at(0), (int)ansi_str.size(), &result.at(0), size_needed);
    return result;
}

std::string UnicodeToANSI(std::wstring utf16_str)
{
    if (utf16_str.empty())
        return std::string();

    const auto size_needed = WideCharToMultiByte(CP_ACP, 0, &utf16_str.at(0), (int)utf16_str.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
    }

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &utf16_str.at(0), (int)utf16_str.size(), &result.at(0), size_needed, nullptr, nullptr);
    return result;
}

std::string UnicodeToUtf8(std::wstring utf16_str)
{
    if (utf16_str.empty())
        return std::string();

    const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &utf16_str.at(0), (int)utf16_str.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
    }

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &utf16_str.at(0), (int)utf16_str.size(), &result.at(0), size_needed, nullptr, nullptr);
    return result;
}

std::string Utf8ToANSI(std::string utf8_str) {
    return UnicodeToANSI(Utf8ToUnicode(utf8_str));
}
std::string ANSIToUtf8(std::string ansi_str) {
    return UnicodeToUtf8(ANSIToUnicode(ansi_str));
}

void OnOcrReadPush(const char* pic_path, void* ocr_response_serialize, int serialize_size)
{
    ocr_protobuf::OcrResponse ocr_response;
    ocr_response.ParseFromArray(ocr_response_serialize, serialize_size);

    nlohmann::json resultj;

    char out_buf[512] = { 0 };
    sprintf(out_buf, "[*] OnReadPush type:%d task id:%d errCode:%d\n", ocr_response.type(), ocr_response.task_id(), ocr_response.err_code());
    //std::cout << out_buf;
    resultj["type"] = ocr_response.type();
    resultj["task_id"] = ocr_response.task_id();
    resultj["err_code"] = ocr_response.err_code();
    resultj["pic_path"] = pic_path;
    resultj["ocr_result"] = nlohmann::json::array();

    sprintf(out_buf, "[*] OnReadPush TaskId:%d -> PicPath:%s\n", ocr_response.task_id(), pic_path);
    //std::cout << out_buf;
    if (ocr_response.type() == 0)
    {
        //std::cout << "[*] OcrResult:\n[\n";
        auto result = ocr_response.ocr_result();
        for (int i = 0; i < result.single_result_size(); i++)
        {
            ocr_protobuf::OcrResponse::OcrResult::SingleResult single_result = result.single_result(i);
            sprintf(out_buf, "\tRECT:[lx:%f, ly:%f, rx:%f, ry:%f]\n", single_result.lx(), single_result.ly(), single_result.rx(), single_result.ry());
            //std::cout << out_buf;
            std::string utf8str = single_result.single_str_utf8();
            //printf("\tUTF8STR:[");
            for (size_t j = 0; j < utf8str.size(); j++)
            {
                //printf("%02X ", (BYTE)utf8str[j]);
            }
            //std::cout << " (" << Utf8ToANSI(utf8str) << ")]\n";

            nlohmann::json item;
            item["rect"] = { //
                (int)round(single_result.lx()), //
                (int)round(single_result.ly()), //
                (int)round(single_result.rx()), //
                (int)round(single_result.ry()) };
            item["utf8str"] = utf8str;
            item["rate"] = single_result.single_rate();
            resultj["ocr_result"].push_back(item);
        }
        //puts("]");

        std::cout << Utf8ToANSI(resultj.dump(2, ' ').c_str()) << "\n";
        g_resultj = resultj;
        if (g_hEvent != NULL) SetEvent(g_hEvent);//让main函数继续运行
    }
    else {
        std::cout << "*" << Utf8ToANSI(resultj.dump(2, ' ').c_str()) << "\n";
    }
/*
{
  "err_code": 0,
  "ocr_result": [],
  "pic_path": "",
  "task_id": 1,
  "type": 1
}
*/
}