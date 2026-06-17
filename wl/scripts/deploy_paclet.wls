#!/usr/bin/env wolframscript
(* deploy_paclet.wls -- build the current-platform library, then deploy the
   THVMLink paclet resource page publicly to the Wolfram Cloud through its
   Resource Definition notebook (ResourceDefinition.nb).  This is the
   scripted equivalent of the notebook's "Deploy -> Publicly in the cloud"
   button: it scrapes the Paclet resource and CloudDeploys it publicly, so
   it installs as a resource (ResourceObject / PacletInstall) carrying the
   repository metadata from docs/ResourceDefinition.md.

   Contrast with publish_paclet.wls, which CopyFile's a packed .paclet to a
   raw public CloudObject (the loadable binary, no resource page).

       wolframscript -f wl/scripts/deploy_paclet.wls

   Build the notebook first with build_docs.wls if it is stale.  Set
   BUILD_NATIVE=0 to skip the `make wl` step.  For non-interactive (CI)
   use, set WOLFRAM_CLOUD_USER / WOLFRAM_CLOUD_PASSWORD.

   Modeled on PureMath/scripts/publish.wls. *)

repoRoot  = FileNameJoin[{DirectoryName[$InputFileName], "..", ".."}]
pacletDir = FileNameJoin[{DirectoryName[$InputFileName], "..", "THVMLink"}]
defNb     = FileNameJoin[{pacletDir, "ResourceDefinition.nb"}]

(* 1. Build the native library for this platform (skip with BUILD_NATIVE=0). *)
If[ Environment["BUILD_NATIVE"] =!= "0",
    proc = RunProcess[{"make", "wl"}, ProcessDirectory -> repoRoot];
    Print[StringTrim[proc["StandardOutput"]]];
    If[ proc["ExitCode"] =!= 0,
        Print[proc["StandardError"]];
        Print["ERROR: `make wl` failed (exit ", proc["ExitCode"], ")"];
        Exit[1]
    ]
]

(* 2. The Resource Definition notebook must exist (build_docs.wls renders
   it from docs/ResourceDefinition.md). *)
If[ ! FileExistsQ[defNb],
    Print["ERROR: definition notebook not found: ", defNb];
    Print["       build it first: wolframscript -f wl/scripts/build_docs.wls"];
    Exit[1]
]

(* 3. Register the paclet so the scrape resolves `Paclet:
   WolframInstitute/THVMLink` by name and bundles its archive (it is not
   otherwise installed in a clean / CI kernel). *)
PacletDirectoryLoad[pacletDir]

(* 4. Cloud session.  CI: WOLFRAM_CLOUD_USER / WOLFRAM_CLOUD_PASSWORD. *)
If[ ! $CloudConnected,
    With[{user = Environment["WOLFRAM_CLOUD_USER"], pass = Environment["WOLFRAM_CLOUD_PASSWORD"]},
        If[ StringQ[user] && StringQ[pass], CloudConnect[user, pass], CloudConnect[]]
    ]
]
If[ ! $CloudConnected,
    Print["ERROR: Wolfram Cloud connection failed (set WOLFRAM_CLOUD_USER / WOLFRAM_CLOUD_PASSWORD for CI)."];
    Exit[1]
]

Needs["DefinitionNotebookClient`"]
Needs["PacletResource`DefinitionNotebook`"]

(* 5. Headless equivalent of "Deploy -> Publicly in the cloud".  The button
   calls DeployResource[nb, "CloudPublic"], which scrapes with Interactive
   -> True; that pops a confirmation dialog, and under wolframscript the
   init-context kernel returns $Canceled and the deploy aborts.  So we do
   what the button does internally: scrape with Interactive -> False, then
   CloudDeploy publicly to <account>/DeployedResources/Paclet/<name>.

   Open the notebook in place with NotebookOpen[defNb] -- NOT File[defNb]
   (unevaluated on Wolfram 14.x) and NOT a temp copy (the notebook
   references the paclet relative to its own directory).  ScrapeResource
   misses the paclet until PacletObject + ScrapePacletDirectory have each
   run once on the freshly-opened notebook. *)
result = UsingFrontEnd @ TimeConstrained[
    Enclose @ Module[{nbo},
        nbo = ConfirmMatch[
            NotebookOpen[defNb, Visible -> False],
            _NotebookObject,
            "opening the definition notebook failed"
        ];
        PacletObject["WolframInstitute/THVMLink"];
        Quiet[PacletResource`DefinitionNotebook`ScrapePacletDirectory[nbo]];
        WithCleanup[
            Module[{res, co},
                res = ConfirmMatch[
                    DefinitionNotebookClient`ScrapeResource[
                        "Paclet", nbo,
                        Interactive -> False,
                        "FailureCondition" -> "Error"
                    ],
                    _ResourceObject,
                    "scraping the resource from the definition notebook failed"
                ];
                (* Deploy to the location the "Deploy -> Publicly" button computes
                   (<account>/DeployedResources/Paclet/<Publisher>/<Name>, slash kept
                   as a PATH separator).  URL-encoding the slash (%2F) instead makes a
                   single flat object whose nested Documentation/* files do not serve --
                   which breaks the statichtml re-hosting build_docs_site.wls relies on.
                   Modeled on PureMath/scripts/publish.wls. *)
                ResourceSystemClient`Private`loadDeployResourceShingle[];
                co = Replace[
                    DeployedResourceShingle`Private`defaultCloudObjectLocation["Paclet", res["Name"]],
                    Except[_CloudObject] :> CloudObject[]
                ];
                With[{obj = CloudDeploy[res, co, Permissions -> "Public"]},
                    ResourceRegister[res, "Cloud", "StoreContent" -> False];
                    obj
                ]
            ],
            NotebookClose[nbo]
        ]
    ],
    420,
    $TimedOut
]

url = FirstCase[{result}, c_CloudObject :> First[c], Missing[], Infinity]

If[ MissingQ[url],
    If[ FailureQ[result],
        Print["ERROR: deploy failed: ", result["Message"]],
        Print["ERROR: deploy did not return a URL.  Result: ", ToString[result]]
    ];
    Exit[1]
]

Print[""]
Print["[deploy] published: ", url]
