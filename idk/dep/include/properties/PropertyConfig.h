//
////--------------------------------------------------------------------------------------------
//// Define the type for the string
////--------------------------------------------------------------------------------------------
//using string_t = std::string;
//
////--------------------------------------------------------------------------------------------
//// Some structure to show that you can add your own atomic structures
////--------------------------------------------------------------------------------------------
//struct oobb
//{
//    float m_Min{}, m_Max{};
//};
//
////--------------------------------------------------------------------------------------------
//// User settings
////--------------------------------------------------------------------------------------------
//namespace property
//{
//    //--------------------------------------------------------------------------------------------
//    // These section provides a basic frame work to for an editor
//    //--------------------------------------------------------------------------------------------
//    namespace editor
//    {
//        // Specifies custom renders for each type
//        struct style_info_base
//        {
//            void*                   m_pDrawFn { nullptr };
//        };
//
//        // Settings for each type when rendering
//        template< typename T >
//        struct style_info : style_info_base{};
//
//        // Custom settings for rendering ints
//        template<>
//        struct style_info<int> : style_info_base
//        {                                       
//            int                     m_Min;
//            int                     m_Max;
//            const char*             m_pFormat;
//            float                   m_Speed;
//        };
//
//        // Custom settings for rendering floats
//        template<>
//        struct style_info<float> : style_info_base
//        {
//            float                   m_Min;
//            float                   m_Max;
//            const char*             m_pFormat;
//            float                   m_Speed;
//            float                   m_Power;
//        };
//    }
//
//    //--------------------------------------------------------------------------------------------
//    // Settings for the property system
//    //--------------------------------------------------------------------------------------------
//    namespace settings
//    {
//        //--------------------------------------------------------------------------------------------
//        // The properties which the property system is going to know about.
//        //--------------------------------------------------------------------------------------------
//        using data_variant = std::variant
//        <
//              int
//            , bool
//            , float
//            , string_t
//            , oobb
//        >;
//    
//        //--------------------------------------------------------------------------------------------
//        // Group all the editor::setting_info under a single varient. We must follow data_variant order
//        // to stay type safe.
//        //--------------------------------------------------------------------------------------------
//        namespace editor
//        {
//            // Helper
//            namespace details
//            {
//                template<typename... T>
//                std::variant< property::editor::style_info<T> ...> CreateEditorEditStyles( std::variant< T...> );
//            }
//
//            // Actual variant with all the different editing styles
//            using styles_info_variant = decltype( details::CreateEditorEditStyles( std::declval<data_variant>() ) );
//        }
//
//        //--------------------------------------------------------------------------------------------
//        // User define data for each property
//        //--------------------------------------------------------------------------------------------
//        struct user_entry
//        {
//            const char*                                 m_pHelp            { nullptr };     // A simple string describing to the editor's user what this property does
//            std::optional<editor::styles_info_variant>  m_EditStylesInfo   {};              // If not style is set then the default will be used
//
//            constexpr user_entry() = default;
//
//            // Function for user to setup a help string for their properties
//            template< typename T = property::setup_entry >                                  // We use a template here to ask the compiler to resolve this function later
//            constexpr  T   Help ( const char* pHelp ) const noexcept                        // Thanks to that we can make this function a constexpr function
//            { 
//                T r = *static_cast<const T*>(this);                                         // Because we are a constexpr function we can not modify the class directly we must copy it
//                r.m_pHelp = pHelp;                                                          // Now we can modify the variable that we care about
//                return r;                                                        // We return our new instance
//            }
//
//            // Setting the editor display style (Look in imGuiPropertyExample for mode details)
//            template< typename T = property::setup_entry >                                  // We use a template here to ask the compiler to resolve this function later
//            constexpr  T   EDStyle ( editor::styles_info_variant Style  ) const noexcept    // Thanks to that we can make this function a constexpr function
//            {
//                T r = *static_cast<const T*>(this);
//                assert( r.m_FunctionTypeGetSet.index() == Style.index() );                  // Make sure that the property type is the same type
//                r.m_EditStylesInfo = Style;
//                return r;
//            }
//        };
//    }
//}

