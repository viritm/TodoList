#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <sqlite3.h>
#include <iostream>
#include <algorithm>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

struct FrameContext
{
    ID3D12CommandAllocator *CommandAllocator;
    UINT64 FenceValue;
};

/**
 * @brief Структура, представляющая задачу в списке задач.
 */
struct Task
{

    std::string name; ///< Название задачи.
    bool is_finished; ///< Флаг, указывающий, выполнена ли задача.

    /**
     * @brief Конструктор для инициализации задачи.
     * @param n Название задачи.
     * @param finished Указание, выполнена ли задача (true - выполнена, false - не выполнена).
     */
    Task(const std::string &n, bool finished) : name(n), is_finished(finished) {}
};

// Data
static int const NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT g_frameIndex = 0;

static int const NUM_BACK_BUFFERS = 3;
static ID3D12Device *g_pd3dDevice = nullptr;
static ID3D12DescriptorHeap *g_pd3dRtvDescHeap = nullptr;
static ID3D12DescriptorHeap *g_pd3dSrvDescHeap = nullptr;
static ID3D12CommandQueue *g_pd3dCommandQueue = nullptr;
static ID3D12GraphicsCommandList *g_pd3dCommandList = nullptr;
static ID3D12Fence *g_fence = nullptr;
static HANDLE g_fenceEvent = nullptr;
static UINT64 g_fenceLastSignaledValue = 0;
static IDXGISwapChain3 *g_pSwapChain = nullptr;
static HANDLE g_hSwapChainWaitableObject = nullptr;
static ID3D12Resource *g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext *WaitForNextFrameResources();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * \brief Добавляет новую задачу в список задач и сохраняет её в базе данных.
 *
 * @param task Название новой задачи.
 */
void add_task(const std::string &task);

/**
 * \brief Загружает задачи из базы данных, присваивая их вектору tasks.
 *
 * Загружаются только задачи с флагом is_finished, установленным в 0.
 */
void load_tasks_from_database();

/**
 * \brief Удаляет все завершенные задачи из вектора tasks и базы данных.
 *
 * Завершенные задачи определяются по флагу is_finished, установленному в true.
 * После удаления обновляет список завершенных задач.
 */
void delete_tasks();

/**
 * \brief Обновляет информацию о завершенности задач в базе данных на основе данных в векторе tasks.
 *
 * Для каждой задачи в векторе tasks обновляет запись в базе данных, устанавливая
 * значение поля task_finished в соответствии с флагом is_finished в задаче.
 *
 * @note Эта функция предполагает, что база данных уже открыта и будет закрыта в конце.
 */
void update_tasks_in_database();

/**
 * \brief Получает список завершенных задач из базы данных.
 *
 * Извлекает из базы данных все записи, у которых значение поля task_finished равно 1 (задачи завершены).
 * Создает и возвращает вектор Task, содержащий информацию о завершенных задачах.
 *
 * @return Вектор Task, содержащий информацию о завершенных задачах из базы данных.
 *         Если произошла ошибка при работе с базой данных, возвращается пустой вектор.
 */
std::vector<Task> get_finished_tasks();

/**
 * \brief Отображает список завершенных задач в пользовательском интерфейсе.
 *
 * Выводит в окно списка завершенных задач каждую завершенную задачу с их порядковыми номерами.
 *
 * @note Для правильного отображения текста задачи используется ImGui::TextWrapped(),
 *       что позволяет автоматически переносить текст на новую строку при необходимости.
 */
void render_finished_list();

/**
 * \brief Очищает базу данных от завершенных задач.
 *
 * Выполняет SQL-запрос для удаления всех завершенных задач (с флагом is_finished = 1) из таблицы tasks в базе данных.
 * После успешного выполнения выводит сообщение об успешной очистке.
 */
void clear_finished_tasks();

/**
 * \brief Создает базу данных и таблицу tasks при их отсутствии.
 *
 * При открытии или создании базы данных проверяет наличие таблицы tasks. Если таблица не существует,
 * выполняет SQL-запрос для ее создания с полями task_name (текстовое поле для имени задачи) и
 * task_finished (целочисленное поле для статуса завершения задачи).
 * Если процесс создания таблицы завершился успешно, база данных закрывается.
 */
void create_database_and_table();

/**
 * \brief Отображает список задач с возможностью отметки выполненных задач.
 *
 * Эта функция отображает список задач в области с прокруткой. Каждая задача представлена чекбоксом,
 * позволяющим отметить задачу как выполненную или невыполненную. Также рядом с чекбоксом выводится
 * порядковый номер задачи и текст задачи. В случае изменения состояния чекбокса, состояние задачи
 * в векторе tasks также обновляется.
 */
void render_task_list();

// My values
std::vector<Task> tasks;          /**< Вектор задач, активных в данный момент. */
std::vector<Task> finished_tasks; /**< Вектор завершенных задач. */

int main(int, char **)
{
    // Create application window
    // ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"TodoList", nullptr};
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"TodoList", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16, NULL, io.Fonts->GetGlyphRangesCyrillic());
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(g_pd3dDevice, NUM_FRAMES_IN_FLIGHT,
                        DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
                        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
                        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // My values
    char task_input[256] = "";

    // Main loop
    bool done = false;
    bool show_finished_list = false;

    load_tasks_from_database();
    finished_tasks = {get_finished_tasks()};

    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        create_database_and_table();

        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(800, 600));
        if (ImGui::Begin(u8"Список задач"))
        {
            render_task_list();
            if (ImGui::Button(u8"Удалить выделенные"))
            {
                delete_tasks();
            }
            ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"Поле для ввода");
            ImGui::InputText(" ", task_input, sizeof(task_input));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.0f, 1.0f));
            if (ImGui::Button(u8"Добавить", ImVec2(76, 28)) || (io.KeysDown[ImGuiKey_Enter] && io.KeyMods == 0))
            {
                if (strlen(task_input) > 0)
                {
                    add_task(task_input);
                    memset(task_input, 0, sizeof(task_input));
                }
            }
            ImGui::PopStyleColor();

            if (ImGui::Button(u8"Показать выполнненые задачи"))
            {
                show_finished_list = !show_finished_list; // Переключение флага при нажатии на кнопку
                ImGui::SameLine();
                ImGui::Spacing();
            }

            if (show_finished_list)
            {
                render_finished_list();
                ImGui::SameLine();
                ImGui::Spacing();
                if (ImGui::Button(u8"Очистить список"))
                {
                    finished_tasks.clear();
                    clear_finished_tasks();
                }
            }
        }
        ImGui::End();

        // Rendering
        ImGui::Render();

        FrameContext *frameCtx = WaitForNextFrameResources();
        UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
        frameCtx->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_mainRenderTargetResource[backBufferIdx];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_pd3dCommandList->Reset(frameCtx->CommandAllocator, nullptr);
        g_pd3dCommandList->ResourceBarrier(1, &barrier);

        // Render Dear ImGui graphics
        const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};
        g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
        g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr);
        g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->Close();

        g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&g_pd3dCommandList);

        g_pSwapChain->Present(1, 0); // Present with vsync
        // g_pSwapChain->Present(0, 0); // Present without vsync

        UINT64 fenceValue = g_fenceLastSignaledValue + 1;
        g_pd3dCommandQueue->Signal(g_fence, fenceValue);
        g_fenceLastSignaledValue = fenceValue;
        frameCtx->FenceValue = fenceValue;
    }

    WaitForLastSubmittedFrame();

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Функция для создания таблицы tasks в базе данных
void create_database_and_table()
{
    const char *db_file = "todo_list.db";

    sqlite3 *db;

    int rc = sqlite3_open(db_file, &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    const char *create_table_query = "CREATE TABLE IF NOT EXISTS tasks ("
                                     "task_name TEXT NOT NULL,"
                                     "task_finished INTEGER NOT NULL"
                                     ");";

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, create_table_query, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare query: %s\n", sqlite3_errmsg(db));
        return;
    }

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    sqlite3_close(db);
}

/**
 *\brief Добавляет новую задачу в список задач и сохраняет её в базе данных.
 *
 *@param task Название новой задачи.
 */
void add_task(const std::string &task)
{

    if (!task.empty())
    {
        Task new_task(task, false);
        tasks.push_back(new_task);

        sqlite3 *db;
        int result = sqlite3_open("todo_list.db", &db);
        if (result == SQLITE_OK)
        {
            std::string sql = "INSERT INTO tasks (task_name, task_finished) VALUES ('" + task + "', 0);";

            result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
            if (result != SQLITE_OK)
            {
                std::cerr << "Error inserting task into database: " << sqlite3_errmsg(db) << std::endl;
            }

            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

            // Закрываем базу данных
            sqlite3_close(db);
        }
        else
        {
            std::cerr << "Error opening database!" << std::endl;
        }
    }
}

void load_tasks_from_database()
{
    sqlite3 *db;
    int result = sqlite3_open("todo_list.db", &db);

    if (result == SQLITE_OK)
    {
        const char *select_query = "SELECT task_name, task_finished FROM tasks WHERE task_finished = 0;";
        sqlite3_stmt *stmt;

        result = sqlite3_prepare_v2(db, select_query, -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        tasks.clear();

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *task_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            bool is_finished = static_cast<bool>(sqlite3_column_int(stmt, 1));
            tasks.push_back(Task(task_name, is_finished));
        }

        sqlite3_finalize(stmt);

        sqlite3_close(db);
    }
    else
    {
        std::cerr << "Error opening database!" << std::endl;
    }
}

void render_finished_list()
{
    ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"Завершенные задачи");
    ImGui::BeginChild("FinishedTaskList", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < finished_tasks.size(); i++)
    {
        std::string task_text = finished_tasks[i].name;
        ImGui::Text("%d.", i + 1);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", task_text.c_str());
    }

    ImGui::EndChild();
}

void render_task_list()
{
    // Область с прокруткой для списка задач
    ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"Актуальные задачи");
    ImGui::BeginChild("TaskList", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < tasks.size(); i++)
    {

        std::string task_text = tasks[i].name;

        bool is_checked = tasks[i].is_finished;
        if (ImGui::Checkbox(("##Checkbox" + std::to_string(i)).c_str(), &is_checked))
        {
            tasks[i].is_finished = is_checked;
        }
        ImGui::SameLine();
        ImGui::Text("%d.", i + 1);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", task_text.c_str());
    }

    ImGui::EndChild();
}

void update_tasks_in_database()
{
    sqlite3 *db;
    int result = sqlite3_open("todo_list.db", &db);

    if (result == SQLITE_OK)
    {

        for (const auto &task : tasks)
        {
            const char *update_query = "UPDATE tasks SET task_finished = ? WHERE task_name = ?;";
            sqlite3_stmt *stmt;

            result = sqlite3_prepare_v2(db, update_query, -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
                return;
            }

            int is_finished = task.is_finished ? 1 : 0;

            const char *task_name = task.name.c_str();

            sqlite3_bind_int(stmt, 1, is_finished);
            sqlite3_bind_text(stmt, 2, task_name, -1, SQLITE_STATIC);

            // Выполняем запрос
            result = sqlite3_step(stmt);

            sqlite3_finalize(stmt);

            if (result != SQLITE_DONE)
            {
                std::cerr << "Failed to update task in database: " << sqlite3_errmsg(db) << std::endl;
                return;
            }
        }

        sqlite3_close(db);
    }
    else
    {
        std::cerr << "Error opening database!" << std::endl;
    }
}

void clear_finished_tasks()
{
    const char *db_file = "todo_list.db";
    sqlite3 *db;

    int rc = sqlite3_open(db_file, &db);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    const char *clear_finished_tasks_query = "DELETE FROM tasks WHERE task_finished = 1;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, clear_finished_tasks_query, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        std::cerr << "Execution failed: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Finished tasks cleared successfully." << std::endl;
}

void delete_tasks()
{
    update_tasks_in_database();
    for (int i = 0; i < tasks.size(); i++)
    {
        if (tasks[i].is_finished == true)
        {
            tasks.erase(tasks.begin() + i);
        }
    }
    finished_tasks = {get_finished_tasks()};
}

std::vector<Task> get_finished_tasks()
{
    const char *db_file = "todo_list.db";

    sqlite3 *db;
    int rc = sqlite3_open(db_file, &db);

    std::vector<Task> finished_tasks;

    if (rc != SQLITE_OK)
    {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return finished_tasks;
    }

    const char *select_query = "SELECT task_name, task_finished FROM tasks WHERE task_finished = 1;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, select_query, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return finished_tasks;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        std::string task_name = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
        bool task_finished = sqlite3_column_int(stmt, 1) == 1;
        Task task(task_name, task_finished);
        finished_tasks.push_back(task);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return finished_tasks;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

    // [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug *pdx12Debug = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

        // [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    if (pdx12Debug != nullptr)
    {
        ID3D12InfoQueue *pInfoQueue = nullptr;
        g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        pInfoQueue->Release();
        pdx12Debug->Release();
    }
#endif

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (g_fenceEvent == nullptr)
        return false;

    {
        IDXGIFactory4 *dxgiFactory = nullptr;
        IDXGISwapChain1 *swapChain1 = nullptr;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
            return false;
        if (dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1) != S_OK)
            return false;
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->SetFullscreenState(false, nullptr);
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_hSwapChainWaitableObject != nullptr)
    {
        CloseHandle(g_hSwapChainWaitableObject);
    }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator)
        {
            g_frameContext[i].CommandAllocator->Release();
            g_frameContext[i].CommandAllocator = nullptr;
        }
    if (g_pd3dCommandQueue)
    {
        g_pd3dCommandQueue->Release();
        g_pd3dCommandQueue = nullptr;
    }
    if (g_pd3dCommandList)
    {
        g_pd3dCommandList->Release();
        g_pd3dCommandList = nullptr;
    }
    if (g_pd3dRtvDescHeap)
    {
        g_pd3dRtvDescHeap->Release();
        g_pd3dRtvDescHeap = nullptr;
    }
    if (g_pd3dSrvDescHeap)
    {
        g_pd3dSrvDescHeap->Release();
        g_pd3dSrvDescHeap = nullptr;
    }
    if (g_fence)
    {
        g_fence->Release();
        g_fence = nullptr;
    }
    if (g_fenceEvent)
    {
        CloseHandle(g_fenceEvent);
        g_fenceEvent = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1 *pDebug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}

void CreateRenderTarget()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource *pBackBuffer = nullptr;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i])
        {
            g_mainRenderTargetResource[i]->Release();
            g_mainRenderTargetResource[i] = nullptr;
        }
}

void WaitForLastSubmittedFrame()
{
    FrameContext *frameCtx = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext *WaitForNextFrameResources()
{
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = {g_hSwapChainWaitableObject, nullptr};
    DWORD numWaitableObjects = 1;

    FrameContext *frameCtx = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtx->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtx;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            WaitForLastSubmittedFrame();
            CleanupRenderTarget();
            HRESULT result = g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
            assert(SUCCEEDED(result) && "Failed to resize swapchain.");
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}