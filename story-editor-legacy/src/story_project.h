#ifndef STORY_PROJECT_H
#define STORY_PROJECT_H

#include <vector>
#include <string>
#include <filesystem>
#include "json.hpp"

// FIXME : Structure très Lunii style, à utiliser pour la conversion peut-être ...
struct StoryNode
{
    bool auto_jump;
    int sound;
    int image;
    int id;
    std::vector<int> jumps;

    std::vector<StoryNode *> children;

    StoryNode& operator=(const StoryNode& other) {
        this->auto_jump = other.auto_jump;
        this->sound = other.sound;
        this->image = other.image;
        this->id = other.id;

        this->jumps.clear();
        this->jumps = other.jumps;
        this->children = other.children;

        return *this;
    }

//            "auto_jump": false,
//            "id": 0,
//            "image": 0,
//            "jumps": [1],
//            "sound": 0
};

struct Resource
{
    std::string file;
    std::string description;
    std::string format;
    std::string type;
};

struct StoryProject
{
    enum ImageFormat { IMG_FORMAT_BMP_4BITS, IMG_FORMAT_QOIF, IMG_FORMAT_COUNT };
    enum SoundFormat { SND_FORMAT_WAV, SND_FORMAT_QOAF, SND_FORMAT_COUNT };

    std::vector<StoryNode> m_nodes;

    std::string m_type;
    std::string m_code;

    StoryNode *m_tree;

    bool Load(const std::string &file_path, nlohmann::json &model);
    void Save(const nlohmann::json &model);

    void CreateTree();
    void Clear() {
        m_uuid = "";
        m_working_dir = "";
        m_resources.clear();
        m_initialized = false;
    }
    
    void SetImageFormat(ImageFormat format);
    void SetSoundFormat(SoundFormat format);
    void SetDisplayFormat(int w, int h);
    void SetName(const std::string &name) { m_name = name; }
    void SetUuid(const std::string &uuid) { m_uuid = uuid; }

    std::string GetProjectFilePath() const;
    std::string GetWorkingDir() const;
    std::string GetName() const { return m_name; }
    std::string GetUuid() const { return m_uuid; }

    std::filesystem::path AssetsPath() const { return m_assetsPath; }

    static std::string GetFileExtension(const std::string &FileName);
    static std::string GetFileName(const std::string &path);
    static std::string RemoveFileExtension(const std::string &FileName);
    static void ReplaceCharacter(std::string &theString, const std::string &toFind, const std::string &toReplace);
    static std::string FileToConstant(const std::string &FileName, const std::string &extension);

    void SetTitleImage(const std::string &titleImage);
    void SetTitleSound(const std::string &titleSound);

    std::string GetTitleImage() const { return m_titleImage; }
    std::string GetTitleSound() const { return m_titleSound; }

    // -------------  Resources Management
    void AppendResource(const Resource &res);
    bool GetResourceAt(int index, Resource &resOut);
    void ClearResources();
    void DeleteResourceAt(int index);
    int ResourcesSize() const { return m_resources.size(); }

    std::vector<Resource>::const_iterator Begin() const { return m_resources.begin(); }
    std::vector<Resource>::const_iterator End() const { return m_resources.end(); }


public:
    // Initialize with an existing project
    void Initialize(const std::string &file_path);
    void New(const std::string &uuid, const std::string &file_path);

    static void EraseString(std::string &theString, const std::string &toErase);
    static std::string ToUpper(const std::string &input);


    void SaveStory(const std::vector<uint8_t> &m_program);
private:
    // Project properties and location
    std::string m_name; /// human readable name
    std::string m_uuid;
    std::filesystem::path m_assetsPath;

    bool m_initialized{false};

    std::string m_titleImage;
    std::string m_titleSound;

    std::vector<Resource> m_resources;
    std::filesystem::path m_working_dir; /// Temporary folder based on the uuid, where the archive is unzipped
    std::string m_project_file_path; /// JSON project file

    int m_display_w{320};
    int m_display_h{240};

    ImageFormat m_imageFormat{IMG_FORMAT_BMP_4BITS};
    SoundFormat m_soundFormat{SND_FORMAT_WAV};
};

#endif // STORY_PROJECT_H
