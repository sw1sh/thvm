# References

Consolidated bibliography for [docs/neuroscience/](README.md). Links
were resolvable as of May 2026. Items flagged "(blog/tech report)"
have no peer-reviewed paper.

## Max Bennett, *A Brief History of Intelligence* (the spine)

- Max S. Bennett, *A Brief History of Intelligence: Evolution, AI,
  and the Five Breakthroughs That Made Our Brains*, Mariner Books,
  2023. Author site: <https://www.abriefhistoryofintelligence.com/>
- Adnan Masood, "The Five Breakthroughs of Intelligence -- a book
  review of Max Bennett's *A Brief History of Intelligence*" (Medium).
  <https://medium.com/@adnanmasood/the-five-breakthroughs-of-intelligence-an-book-review-of-max-bennetts-a-brief-history-of-91db89f9f3c4>
- "A History of Intelligence" -- Stetson, *Holodoxa* (Substack).
  <https://stetson.substack.com/p/a-history-of-intelligence>
- Reading notes: <https://scyy.fi/shelf/history-of-intelligence>
- Bennett discusses the book on the *Brain Inspired* podcast (search
  "Brain Inspired Max Bennett").

## Neuroscience basics

- Eric R. Kandel et al., *Principles of Neural Science*, 6th ed.,
  McGraw-Hill, 2021.
- Peter Dayan & L. F. Abbott, *Theoretical Neuroscience: Computational
  and Mathematical Modeling of Neural Systems*, MIT Press, 2001.
- Wolfram Schultz, Peter Dayan & P. Read Montague, "A neural substrate
  of prediction and reward", *Science* 275:1593-1599 (1997).
  <https://www.science.org/doi/10.1126/science.275.5306.1593>
- Andre M. Bastos et al., "Canonical microcircuits for predictive
  coding", *Neuron* 76(4):695-711 (2012).
  <https://doi.org/10.1016/j.neuron.2012.10.038>

## Reinforcement learning and dopamine (breakthrough 2)

- Richard S. Sutton & Andrew G. Barto, *Reinforcement Learning: An
  Introduction*, 2nd ed., MIT Press, 2018.
  <http://incompleteideas.net/book/the-book-2nd.html>
- Will Dabney, Zeb Kurth-Nelson, Naoshige Uchida, Clara Kwon
  Starkweather, Demis Hassabis, Remi Munos & Matthew Botvinick,
  "A distributional code for value in dopamine-based reinforcement
  learning", *Nature* 577:671-675 (2020).
  <https://doi.org/10.1038/s41586-019-1924-6>
- "Distributional reinforcement learning in prefrontal cortex",
  *Nature Neuroscience* (2023). <https://doi.org/10.1038/s41593-023-01535-w>
- Deepak Pathak et al., "Curiosity-driven exploration by
  self-supervised prediction", ICML 2017. <https://arxiv.org/abs/1705.05363>
- John E. Lisman & Anthony A. Grace, "The Hippocampal-VTA Loop:
  Controlling the Entry of Information into Long-Term Memory",
  *Neuron* 46(5):703-713 (2005).
  <https://doi.org/10.1016/j.neuron.2005.05.002> (novelty signalled to
  the dopamine system; the novelty-bonus substrate).
- Kent C. Berridge & Terry E. Robinson, "Parsing reward" (incentive
  salience -- "wanting" vs "liking"), *Trends in Neurosciences*
  26(9):507-513 (2003). <https://doi.org/10.1016/S0166-2236(03)00233-9>
- Brad E. Pfeiffer & David J. Foster, "Hippocampal place-cell sequences
  depict future paths to remembered goals", *Nature* 497:74-79 (2013).
  <https://doi.org/10.1038/nature12112> (goal-directed forward
  simulation -- vicarious trial-and-error; cf. A. David Redish's work
  on deliberation).
- Kimberly L. Stachenfeld, Matthew M. Botvinick & Samuel J. Gershman,
  "The hippocampus as a predictive map", *Nature Neuroscience*
  20:1643-1653 (2017). <https://doi.org/10.1038/nn.4650>
- Samuel J. Gershman, "The successor representation: its computational
  logic and neural substrates", *J. Neuroscience* 38(33):7193-7200
  (2018). <https://doi.org/10.1523/JNEUROSCI.0151-18.2018>

## Generative models, predictive coding, world models (breakthrough 3)

- Peter Dayan, Geoffrey E. Hinton, Radford M. Neal & Richard S. Zemel,
  "The Helmholtz machine", *Neural Computation* 7(5):889-904 (1995).
  <https://doi.org/10.1162/neco.1995.7.5.889>
- Geoffrey E. Hinton, Peter Dayan, Brendan J. Frey & Radford M. Neal,
  "The wake-sleep algorithm for unsupervised neural networks",
  *Science* 268:1158-1161 (1995).
  <https://doi.org/10.1126/science.7761831>
- Diederik P. Kingma & Max Welling, "Auto-Encoding Variational Bayes"
  (the VAE), 2013; arXiv:1312.6114. <https://arxiv.org/abs/1312.6114>
- Jorg Bornschein & Yoshua Bengio, "Reweighted Wake-Sleep", ICLR 2015;
  arXiv:1406.2751. <https://arxiv.org/abs/1406.2751>
- James L. McClelland, Bruce L. McNaughton & Randall C. O'Reilly,
  "Why there are complementary learning systems in the hippocampus and
  neocortex: insights from the successes and failures of connectionist
  models of learning and memory", *Psychological Review* 102(3):
  419-457 (1995). <https://doi.org/10.1037/0033-295X.102.3.419>
- Francis Crick & Graeme Mitchison, "The function of dream sleep",
  *Nature* 304:111-114 (1983). <https://doi.org/10.1038/304111a0>
- Yann LeCun, "A Path Towards Autonomous Machine Intelligence"
  (v0.9.2), OpenReview, 2022.
  <https://openreview.net/pdf?id=BZ5a1r-kVsf>
- Anna Dawid & Yann LeCun, "Introduction to Latent Variable
  Energy-Based Models: A Path Towards Autonomous Machine
  Intelligence", arXiv:2306.02572 (2023). <https://arxiv.org/abs/2306.02572>
- Mahmoud Assran et al., "Self-Supervised Learning from Images with a
  Joint-Embedding Predictive Architecture" (I-JEPA), CVPR 2023;
  arXiv:2301.08243. <https://arxiv.org/abs/2301.08243> ·
  code <https://github.com/facebookresearch/ijepa>
- Adrien Bardes et al., "Revisiting Feature Prediction for Learning
  Visual Representations from Video" (V-JEPA), 2024;
  arXiv:2404.08471. <https://arxiv.org/abs/2404.08471> ·
  code <https://github.com/facebookresearch/jepa>
- Meta FAIR, "V-JEPA 2: Self-Supervised Video Models Enable
  Understanding, Prediction and Planning", arXiv:2506.09985 (2025).
  <https://arxiv.org/abs/2506.09985> · blog
  <https://ai.meta.com/blog/v-jepa-2-world-model-benchmarks/> ·
  code <https://github.com/facebookresearch/vjepa2>
- Adrien Bardes, Jean Ponce & Yann LeCun, "VICReg: Variance-Invariance-
  Covariance Regularization for Self-Supervised Learning", ICLR 2022;
  arXiv:2105.04906. <https://arxiv.org/abs/2105.04906> (the classic
  joint-embedding anti-collapse regulariser).
- Randall Balestriero & Yann LeCun, "LeJEPA: Provable and Scalable
  Self-Supervised Learning Without the Heuristics" (introduces SIGReg,
  the sketched isotropic-Gaussian regulariser), 2025 (search arXiv by
  title; ID to confirm). Used in the page-9 follow-up on non-collapsing
  latent-prediction-error curiosity.
- Randall Balestriero & Yann LeCun, "LeJEPA: Provable and Scalable
  Self-Supervised Learning Without the Heuristics", arXiv:2511.08544
  (Nov 2025). <https://arxiv.org/abs/2511.08544> ·
  code <https://github.com/rbalestr-lab/lejepa>
- Lucas Maes, Quentin Le Lidec, Damien Scieur, Yann LeCun & Randall
  Balestriero, "LeWorldModel: Stable End-to-End Joint-Embedding
  Predictive Architecture from Pixels", arXiv:2603.19312 (Mar 2026).
  <https://arxiv.org/abs/2603.19312> · page <https://le-wm.github.io/> ·
  code <https://github.com/lucas-maes/le-wm>
- Zhaoyang Dong et al., "Brain-JEPA: Brain Dynamics Foundation Model
  with Gradient Positioning and Spatiotemporal Masking", NeurIPS 2024
  (spotlight); arXiv:2409.19407. <https://arxiv.org/abs/2409.19407> ·
  code <https://github.com/Eric-LRL/Brain-JEPA>
- Adrien Bardes et al. / J. Bachmann et al. (VICReg): Adrien Bardes,
  Jean Ponce & Yann LeCun, "VICReg: Variance-Invariance-Covariance
  Regularization for Self-Supervised Learning", ICLR 2022;
  arXiv:2105.04906. <https://arxiv.org/abs/2105.04906>
- David Ha & Jurgen Schmidhuber, "World Models", 2018;
  arXiv:1803.10122. <https://arxiv.org/abs/1803.10122>
- Danijar Hafner, Jurgis Pasukonis, Jimmy Ba & Timothy Lillicrap,
  "Mastering Diverse Domains through World Models" (DreamerV3),
  arXiv:2301.04104 (2023); published as "Mastering diverse control
  tasks through world models", *Nature* (2025).
  <https://arxiv.org/abs/2301.04104> · code <https://github.com/danijar/dreamerv3>
- Danijar Hafner, Timothy Lillicrap, Mohammad Norouzi & Jimmy Ba,
  "Mastering Atari with Discrete World Models" (DreamerV2), ICLR
  2021; arXiv:2010.02193. <https://arxiv.org/abs/2010.02193> -- the
  discrete (categorical) latent the multi-game world models below
  inherit.
- Kuang-Huei Lee, Ofir Nachum, Mengjiao Yang et al., "Multi-Game
  Decision Transformers", NeurIPS 2022; arXiv:2205.15241.
  <https://arxiv.org/abs/2205.15241> -- one return-conditioned
  transformer across 41 Atari games.
- "Scaling Offline Model-Based RL via Jointly-Optimized World-Action
  Model Pretraining" (JOWA), arXiv:2410.00564 (2024).
  <https://arxiv.org/abs/2410.00564> -- a single VQ-tokenised
  world-action model trained jointly across Atari games.
- "Mixture-of-World Models: Scaling Multi-Task Reinforcement Learning
  with Modular Latent Dynamics", arXiv:2602.01270 (2026).
  <https://arxiv.org/abs/2602.01270> -- one agent over 26 Atari
  games via task-conditioned dynamics experts on a shared backbone.
- Jake Bruce et al., "Genie: Generative Interactive Environments",
  ICML 2024; arXiv:2402.15391. <https://arxiv.org/abs/2402.15391>
- DeepMind, "Genie 2: A large-scale foundation world model" (blog/tech
  report, Dec 2024). <https://deepmind.google/blog/genie-2-a-large-scale-foundation-world-model/>
- DeepMind, "Genie 3: A new frontier for world models" (blog/tech
  report, Aug 2025). <https://deepmind.google/blog/genie-3-a-new-frontier-for-world-models/>
- NVIDIA, "Cosmos World Foundation Model Platform for Physical AI",
  arXiv:2501.03575 (2025). <https://arxiv.org/abs/2501.03575> ·
  <https://www.nvidia.com/en-us/ai/cosmos/>
- OpenAI, "Video generation models as world simulators" (Sora
  technical report, Feb 2024).
  <https://openai.com/index/video-generation-models-as-world-simulators/>
- "Is Sora a World Simulator? A Comprehensive Survey on General World
  Models and Beyond", arXiv:2405.03520 (2024).
  <https://arxiv.org/abs/2405.03520>
- NVIDIA, "SANA-WM: Efficient Minute-Scale World Modeling with Hybrid
  Linear Diffusion Transformer", arXiv:2605.15178 (May 2026).
  <https://arxiv.org/abs/2605.15178> · page
  <https://nvlabs.github.io/Sana/WM/>
- Yifan Wang & Tong He, "Warp-as-History: Generalizable
  Camera-Controlled Video Generation from One Training Video",
  arXiv:2605.15182 (May 2026). <https://arxiv.org/abs/2605.15182> ·
  page <https://yyfz.github.io/warp-as-history/> · code
  <https://github.com/yyfz/Warp-as-History>

## NeuroAI, predictive coding, active inference (page 4)

- Anthony Zador et al., "Catalyzing next-generation Artificial
  Intelligence through NeuroAI", *Nature Communications* 14:1597
  (2023). <https://doi.org/10.1038/s41467-023-37180-x>
  (Preprint: "Toward Next-Generation Artificial Intelligence:
  Catalyzing the NeuroAI Revolution", arXiv:2210.08340, 2022.)
- Adrien Doerig et al., "The neuroconnectionist research programme",
  *Nature Reviews Neuroscience* 24:431-450 (2023).
  <https://doi.org/10.1038/s41583-023-00705-w>
- Beren Millidge, Tommaso Salvatori, Yuhang Song, Rafal Bogacz &
  Thomas Lukasiewicz, "Predictive Coding: Towards a Future of Deep
  Learning beyond Backpropagation?", IJCAI 2022; arXiv:2202.09467.
  <https://arxiv.org/abs/2202.09467>
- "Introduction to Predictive Coding Networks for Machine Learning",
  arXiv:2506.06332 (2025). <https://arxiv.org/abs/2506.06332>
- Geoffrey Hinton, "The Forward-Forward Algorithm: Some Preliminary
  Investigations", 2022; arXiv:2212.13345. <https://arxiv.org/abs/2212.13345>
- Karl Friston, "The free-energy principle: a unified brain theory?",
  *Nature Reviews Neuroscience* 11:127-138 (2010).
  <https://doi.org/10.1038/nrn2787>
- Takuya Isomura et al., "Experimental validation of the free-energy
  principle with in vitro neural networks", *Nature Communications*
  (2023). <https://doi.org/10.1038/s41467-023-40141-z>
- Giovanni Pezzulo, Thomas Parr & Karl Friston, "Generating meaning:
  active inference and the scope and limits of passive AI", *Trends in
  Cognitive Sciences* 28(2):97-112 (2024).
  <https://doi.org/10.1016/j.tics.2023.10.002>

## Hippocampus and transformers (page 4)

- James C. R. Whittington et al., "The Tolman-Eichenbaum Machine:
  Unifying Space and Relational Memory through Generalization in the
  Hippocampal Formation", *Cell* 183(5):1249-1263 (2020).
  <https://doi.org/10.1016/j.cell.2020.10.024>
- James C. R. Whittington, Joseph Warren & Timothy E. J. Behrens,
  "Relating transformers to models and neural representations of the
  hippocampal formation", ICLR 2022; arXiv:2112.04035.
  <https://arxiv.org/abs/2112.04035>

## Mentalizing AI: theory of mind, imitation, opponent modelling (page 4)

- Chris L. Baker, Rebecca Saxe & Joshua B. Tenenbaum, "Action
  understanding as inverse planning", *Cognition* 113(3):329-349
  (2009). <https://doi.org/10.1016/j.cognition.2009.07.005>
- Chris L. Baker, Julian Jara-Ettinger, Rebecca Saxe & Joshua B.
  Tenenbaum, "Rational quantitative attribution of beliefs, desires
  and percepts in human mentalizing" (Bayesian ToM), *Nature Human
  Behaviour* 1:0064 (2017). <https://doi.org/10.1038/s41562-017-0064>
- Neil C. Rabinowitz, Frank Perbet, H. Francis Song, Chiyuan Zhang,
  S. M. Ali Eslami & Matthew Botvinick, "Machine Theory of Mind"
  (ToMnet), ICML 2018; arXiv:1802.07740.
  <https://arxiv.org/abs/1802.07740>
- Anton Bakhtin, Noam Brown, Emily Dinan, Gabriele Farina et al.
  (Meta Fundamental AI Research Diplomacy Team), "Human-level play
  in the game of Diplomacy by combining language models with
  strategic reasoning" (Cicero), *Science* 378(6624):1067-1074 (Nov
  2022). <https://doi.org/10.1126/science.ade9097> ·
  blog <https://ai.meta.com/blog/cicero-ai-negotiates-persuades-and-cooperates-with-people/>
- Michal Kosinski, "Theory of Mind May Have Spontaneously Emerged
  in Large Language Models", arXiv:2302.02083 (Feb 2023); published
  as "Evaluating large language models in theory of mind tasks",
  *PNAS* (2024). <https://arxiv.org/abs/2302.02083> ·
  <https://www.pnas.org/doi/10.1073/pnas.2405460121>
- Tomer Ullman, "Large Language Models Fail on Trivial Alterations
  to Theory-of-Mind Tasks", arXiv:2302.08399 (Feb 2023).
  <https://arxiv.org/abs/2302.08399>
- James W. A. Strachan, Dalila Albergo, Giulia Borghini et al.,
  "Testing theory of mind in large language models and humans",
  *Nature Human Behaviour* 8:1285-1295 (2024).
  <https://doi.org/10.1038/s41562-024-01882-z>
- "Dissecting the Ullman Variations with a SCALPEL: Why do LLMs fail
  at Trivial Alterations to the False Belief Task?", arXiv:2406.14737
  (2024). <https://arxiv.org/abs/2406.14737>
- Maarten Sap, Ronan Le Bras, Daniel Fried & Yejin Choi, "Neural
  Theory-of-Mind? On the Limits of Social Intelligence in Large
  LMs", EMNLP 2022; arXiv:2210.13312.
  <https://arxiv.org/abs/2210.13312>
- Hyunwoo Kim et al., "FANToM: A Benchmark for Stress-testing
  Machine Theory of Mind in Interactions", EMNLP 2023;
  arXiv:2310.15421. <https://arxiv.org/abs/2310.15421>
- Joon Sung Park, Joseph C. O'Brien, Carrie J. Cai, Meredith
  Ringel Morris, Percy Liang & Michael S. Bernstein, "Generative
  Agents: Interactive Simulacra of Human Behavior", UIST 2023;
  arXiv:2304.03442. <https://arxiv.org/abs/2304.03442>
- Michael C. Frank & Noah D. Goodman, "Predicting Pragmatic
  Reasoning in Language Games" (Rational Speech Acts), *Science*
  336(6084):998 (2012). <https://doi.org/10.1126/science.1218633>
- Andrew Y. Ng & Stuart Russell, "Algorithms for Inverse
  Reinforcement Learning", ICML 2000.
  <https://ai.stanford.edu/~ang/papers/icml00-irl.pdf>
- Pieter Abbeel & Andrew Y. Ng, "Apprenticeship Learning via Inverse
  Reinforcement Learning", ICML 2004.
  <https://ai.stanford.edu/~ang/papers/icml04-apprentice.pdf>
- Brian D. Ziebart, Andrew Maas, J. Andrew Bagnell & Anind K. Dey,
  "Maximum Entropy Inverse Reinforcement Learning", AAAI 2008.
  <https://www.aaai.org/Papers/AAAI/2008/AAAI08-227.pdf>
- Jonathan Ho & Stefano Ermon, "Generative Adversarial Imitation
  Learning" (GAIL), NeurIPS 2016; arXiv:1606.03476.
  <https://arxiv.org/abs/1606.03476>
- Cheng Chi, Siyuan Feng, Yilun Du, Zhenjia Xu, Eric Cousineau,
  Benjamin Burchfiel & Shuran Song, "Diffusion Policy: Visuomotor
  Policy Learning via Action Diffusion", RSS 2023; arXiv:2303.04137.
  <https://arxiv.org/abs/2303.04137>
- Tony Z. Zhao et al., "Learning Fine-Grained Bimanual Manipulation
  with Low-Cost Hardware" (ALOHA / ACT), RSS 2023; arXiv:2304.13705.
  <https://arxiv.org/abs/2304.13705>
- Anthony Brohan et al., "RT-2: Vision-Language-Action Models",
  CoRL 2023; arXiv:2307.15818. <https://arxiv.org/abs/2307.15818>
- Octo Model Team, "Octo: An Open-Source Generalist Robot Policy",
  RSS 2024; arXiv:2405.12213. <https://arxiv.org/abs/2405.12213>
- Moo Jin Kim et al., "OpenVLA: An Open-Source Vision-Language-Action
  Model", 2024; arXiv:2406.09246. <https://arxiv.org/abs/2406.09246>
- Oriol Vinyals et al., "Grandmaster level in StarCraft II using
  multi-agent reinforcement learning" (AlphaStar), *Nature*
  575:350-354 (2019). <https://doi.org/10.1038/s41586-019-1724-z>
- Noam Brown & Tuomas Sandholm, "Superhuman AI for multiplayer
  poker" (Pluribus), *Science* 365(6456):885-890 (2019).
  <https://doi.org/10.1126/science.aay2400>
- Jakob N. Foerster, Richard Y. Chen, Maruan Al-Shedivat, Shimon
  Whiteson, Pieter Abbeel & Igor Mordatch, "Learning with
  Opponent-Learning Awareness" (LOLA), AAMAS 2018; arXiv:1709.04326.
  <https://arxiv.org/abs/1709.04326>
- Alexandre Alahi, Kratarth Goel, Vignesh Ramanathan, Alexandre
  Robicquet, Li Fei-Fei & Silvio Savarese, "Social LSTM: Human
  Trajectory Prediction in Crowded Spaces", CVPR 2016.
  <https://doi.org/10.1109/CVPR.2016.110>
- Tim Salzmann, Boris Ivanovic, Punarjay Chakravarty & Marco
  Pavone, "Trajectron++: Dynamically-Feasible Trajectory Forecasting
  with Heterogeneous Data", ECCV 2020; arXiv:2001.03093.
  <https://arxiv.org/abs/2001.03093>

## Meta-RL, prefrontal cortex, learned learning rules (pages 2, 4)

- Jane X. Wang et al., "Prefrontal cortex as a meta-reinforcement
  learning system", *Nature Neuroscience* 21:860-868 (2018).
  <https://doi.org/10.1038/s41593-018-0147-8>
- Jane X. Wang et al., "Learning to reinforcement learn"
  (RL^2-adjacent), 2016; arXiv:1611.05763. <https://arxiv.org/abs/1611.05763>
- Matthew Botvinick et al., "Deep reinforcement learning and its
  neuroscientific implications", *Neuron* 107(4):603-616 (2020).
  <https://doi.org/10.1016/j.neuron.2020.06.014>
- Junhyuk Oh et al., "Discovering state-of-the-art reinforcement
  learning algorithms" (DiscoRL), *Nature* (2025).
  <https://doi.org/10.1038/s41586-025-09761-x>
- Neil C. Rabinowitz et al., "Machine Theory of Mind" (ToMnet), ICML
  2018; arXiv:1802.07740. <https://arxiv.org/abs/1802.07740>

## Whole-brain functional models

- Chris Eliasmith et al., "A large-scale model of the functioning
  brain" (Spaun), *Science* 338:1202-1205 (2012).
  <https://doi.org/10.1126/science.1225266>
- Chris Eliasmith, *How to Build a Brain: A Neural Architecture for
  Biological Cognition*, Oxford University Press, 2013. (Nengo:
  <https://www.nengo.ai/>)

## Numenta, HTM, Thousand Brains, and cousins (page 4 -- cortical columns)

- Jeff Hawkins (with Sandra Blakeslee), *On Intelligence*, Times
  Books / Henry Holt, 2004.
- Jeff Hawkins, *A Thousand Brains: A New Theory of Intelligence*,
  Basic Books, 2021. <https://www.numenta.com/a-thousand-brains-by-jeff-hawkins/>
- Numenta technical blog index: <https://www.numenta.com/blog/>
  Research papers index:
  <https://www.numenta.com/resources/research-publications/papers/>
- Jeff Hawkins, Subutai Ahmad & Yuwei Cui, "A theory of how columns in
  the neocortex enable learning the structure of the world",
  *Frontiers in Neural Circuits* 11:81 (2017). DOI: 10.3389/fncir.2017.00081.
  <https://doi.org/10.3389/fncir.2017.00081>
- Jeff Hawkins, Marcus Lewis, Mirko Klukas, Scott Purdy & Subutai
  Ahmad, "A Framework for Intelligence and Cortical Function Based on
  Grid Cells in the Neocortex", *Frontiers in Neural Circuits* 12:121
  (2019). <https://doi.org/10.3389/fncir.2018.00121>
- Marcus Lewis, Scott Purdy, Subutai Ahmad & Jeff Hawkins,
  "Locations in the Neocortex: A Theory of Sensorimotor Object
  Recognition Using Cortical Grid Cells", *Frontiers in Neural
  Circuits* 13:22 (2019). <https://doi.org/10.3389/fncir.2019.00022>
- Niels Leadholm, Marcus Lewis, Viviane Clay, Hojae Lee, Karan
  Grewal, Scott Purdy, Stratton Long, Tristan Stocker, Heiko
  Hoffmann, Subutai Ahmad & Jeff Hawkins, "The Thousand Brains
  Project: A New Paradigm for Sensorimotor Intelligence",
  arXiv:2412.18354 (Dec 2024).
  <https://arxiv.org/abs/2412.18354> ·
  project home <https://thousandbrains.org/> ·
  Monty code (open source): <https://github.com/thousandbrainsproject>
- Numenta, "Sparsity Enables 100x Performance Acceleration in Deep
  Learning Networks" (whitepaper, 2023).
  <https://www.numenta.com/assets/pdf/research-publications/papers/Sparsity-Enables-100x-Performance-Acceleration-Deep-Learning-Networks.pdf>
- Bruno A. Olshausen & David J. Field, "Emergence of simple-cell
  receptive field properties by learning a sparse code for natural
  images", *Nature* 381:607-609 (1996).
  <https://doi.org/10.1038/381607a0>
- Pentti Kanerva, "Hyperdimensional computing: an introduction to
  computing in distributed representation with high-dimensional random
  vectors", *Cognitive Computation* 1:139-159 (2009).
  <https://doi.org/10.1007/s12559-009-9009-8>
- Denis Kleyko, Dmitri A. Rachkovskij, Evgeny Osipov & Abbas Rahimi,
  "Vector Symbolic Architectures as a Computing Framework for
  Emerging Hardware", *Proceedings of the IEEE* 110(10):1538-1571
  (2022); arXiv:2106.05268. <https://arxiv.org/abs/2106.05268>
- Tony A. Plate, "Holographic Reduced Representations", *IEEE
  Transactions on Neural Networks* 6(3):623-641 (1995).
  <https://doi.org/10.1109/72.377968>
- Paul A. Merolla et al., "A million spiking-neuron integrated
  circuit with a scalable communication network and interface"
  (TrueNorth), *Science* 345:668-673 (2014).
  <https://doi.org/10.1126/science.1254642>
- Mike Davies et al., "Loihi: A Neuromorphic Manycore Processor with
  On-Chip Learning", *IEEE Micro* 38(1):82-99 (2018).
  <https://doi.org/10.1109/MM.2018.112130359>
- Mike Davies et al., "Advancing Neuromorphic Computing With Loihi: A
  Survey of Results and Outlook", *Proceedings of the IEEE* 109(5)
  (2021). <https://doi.org/10.1109/JPROC.2021.3067593>
- Steve Furber et al., "The SpiNNaker Project", *Proceedings of the
  IEEE* 102(5):652-665 (2014).
  <https://doi.org/10.1109/JPROC.2014.2304638>
- Sara Sabour, Nicholas Frosst & Geoffrey E. Hinton, "Dynamic Routing
  Between Capsules" (capsule networks), NeurIPS 2017; arXiv:1710.09829.
  <https://arxiv.org/abs/1710.09829>
- Geoffrey E. Hinton, "How to represent part-whole hierarchies in a
  neural network" (GLOM), 2021; arXiv:2102.12627.
  <https://arxiv.org/abs/2102.12627>

## Sakana AI: evolution + collective intelligence (page 4)

Sakana AI is the most prolific current engineering programme on the
evolutionaries-tribe + collective-intelligence axis. Selected papers
relevant to the brain-AI thread:

- Luke Darlow, Ciaran Regan, Sebastian Risi, Jeffrey Seely, David
  Ha et al., "Continuous Thought Machines" (CTM), arXiv:2505.05522
  (May 2025). <https://arxiv.org/abs/2505.05522> ·
  page <https://pub.sakana.ai/ctm/> ·
  code <https://github.com/SakanaAI/continuous-thought-machines>
- Edoardo Cetin, Qi Sun, Tianyu Tang, David Ha, Robert Tjarko Lange
  et al., "An Evolved Universal Transformer Memory" (NAMM, Neural
  Attention Memory Models), arXiv:2410.13166 (Oct 2024; ICLR 2025).
  <https://arxiv.org/abs/2410.13166> · blog <https://sakana.ai/namm/>
- Robert Tjarko Lange et al., "ShinkaEvolve: Towards Open-Ended and
  Sample-Efficient Program Evolution", arXiv:2509.19349 (Sep 2025).
  <https://arxiv.org/abs/2509.19349> ·
  code <https://github.com/SakanaAI/ShinkaEvolve>
- Jenny Zhang, Shengran Hu, Cong Lu et al., "Darwin-Godel Machine:
  Open-Ended Evolution of Self-Improving Agents" (DGM),
  arXiv:2505.22954 (May 2025). <https://arxiv.org/abs/2505.22954> ·
  blog <https://sakana.ai/dgm/>
- Chris Lu, Samuel Holt, Claudio Fanconi et al., "Discovering
  Preference Optimization Algorithms with and for Large Language
  Models" (DiscoPOP / LLM-squared), arXiv:2406.08414 (Jun 2024).
  <https://arxiv.org/abs/2406.08414> ·
  code <https://github.com/SakanaAI/DiscoPOP>
- Robert Tjarko Lange et al., "Large Language Models as Evolution
  Strategies" (EvoLLM), arXiv:2402.18381 (Feb 2024).
  <https://arxiv.org/abs/2402.18381>
- Yuichi Inoue, Kou Misaki, Yuki Imajuku, Daichi Murata, So Kuroki
  et al., "AB-MCTS: Adaptive Branching Monte Carlo Tree Search for
  Inference-Time Scaling", arXiv:2503.04412 (Mar 2025).
  <https://arxiv.org/abs/2503.04412> · blog <https://sakana.ai/ab-mcts/>
- Sakana AI, "Trinity / Conductor: a 7B model trained to orchestrate
  LLMs", ICLR 2026; arXiv:2512.04695 (Dec 2025).
  <https://arxiv.org/abs/2512.04695>
- Takuya Akiba, Makoto Shing, Yujin Tang, Qi Sun & David Ha,
  "Evolutionary Optimization of Model Merging Recipes", *Nature
  Machine Intelligence* (2025); blog
  <https://sakana.ai/evolutionary-model-merge/> ·
  code <https://github.com/SakanaAI/evolutionary-model-merge>
- So Kuroki, Taishi Nakamura, Takuya Akiba & Yujin Tang, "CycleQD:
  Agent Skill Acquisition through Cyclic Quality-Diversity",
  arXiv:2410.14735 (Oct 2024; NeurIPS 2024 / ICLR 2025).
  <https://arxiv.org/abs/2410.14735>
- Yuki Imajuku et al., "The AI Scientist v2: Workshop-Level Automated
  Scientific Discovery via Agentic Tree Search", arXiv:2504.08066
  (Apr 2025). <https://arxiv.org/abs/2504.08066> ·
  code <https://github.com/SakanaAI/AI-Scientist-v2> · the v1 line +
  *Nature* milestone: <https://sakana.ai/ai-scientist-nature/>
- Makoto Shing, Kou Misaki, Han Bao, Sho Yokoi, Takuya Akiba,
  "Temporally Adaptive Interpolated Distillation" (TAID), ICLR 2025.
  Blog <https://sakana.ai/taid/>
- Edoardo Cetin et al., "Reinforcement-Learned Teachers" (RLT),
  arXiv:2506.08388 (Jun 2025). <https://arxiv.org/abs/2506.08388> ·
  blog <https://sakana.ai/rlt/>
- AI CUDA Engineer postmortem (reward-hacking case study, Feb 2025):
  <https://sakana.ai/ai-cuda-engineer/> ·
  <https://airevolution.poltextlab.com/controversies-surrounding-sakana-ais-cuda-engineer-framework/>

Adjacent context for CTM's "thinking takes time" / synchronisation
framing:

- Wolf Singer, "Neuronal synchrony: a versatile code for the
  definition of relations?", *Neuron* 24:49-65 (1999).
  <https://doi.org/10.1016/s0896-6273(00)80821-1>
- Pascal Fries, "A mechanism for cognitive dynamics: neuronal
  communication through neuronal coherence", *Trends in Cognitive
  Sciences* 9(10):474-480 (2005).
  <https://doi.org/10.1016/j.tics.2005.08.011>
  (Binding-by-synchrony / gamma-band coherence -- the neuroscience
  CTM's synchronisation latent draws on.)

## Classical ML pillars (page 6, part 1)

- Pedro Domingos, *The Master Algorithm: How the Quest for the
  Ultimate Learning Machine Will Remake Our World*, Basic Books, 2015.
  Author talk + slides: <https://learning.acm.org/techtalks/machinelearning>
  ("The Five Tribes of Machine Learning").
- Vladimir N. Vapnik, *The Nature of Statistical Learning Theory*,
  Springer, 1995 (2nd ed. 2000); *Statistical Learning Theory*, Wiley,
  1998. VC-dimension, structural risk minimization, the SVM origin.
- Corinna Cortes & Vladimir Vapnik, "Support-vector networks",
  *Machine Learning* 20(3):273-297 (1995).
  <https://doi.org/10.1007/BF00994018>
- John H. Holland, *Adaptation in Natural and Artificial Systems*,
  University of Michigan Press, 1975. (Evolutionaries.)
- John R. Koza, *Genetic Programming: On the Programming of Computers
  by Means of Natural Selection*, MIT Press, 1992.
- Judea Pearl, *Probabilistic Reasoning in Intelligent Systems:
  Networks of Plausible Inference*, Morgan Kaufmann, 1988.
- David E. Rumelhart, Geoffrey E. Hinton & Ronald J. Williams,
  "Learning representations by back-propagating errors", *Nature*
  323:533-536 (1986). <https://doi.org/10.1038/323533a0>
- J. Ross Quinlan, "Induction of decision trees", *Machine Learning*
  1(1):81-106 (1986). <https://doi.org/10.1007/BF00116251>
- Mikhail Belkin, Daniel Hsu, Siyuan Ma & Soumik Mandal, "Reconciling
  modern machine-learning practice and the classical bias-variance
  trade-off" (double descent), *PNAS* 116(32):15849-15854 (2019).
  <https://doi.org/10.1073/pnas.1903070116>
- Arthur Jacot, Franck Gabriel & Clement Hongler, "Neural Tangent
  Kernel: Convergence and Generalization in Neural Networks", NeurIPS
  2018; arXiv:1806.07572. <https://arxiv.org/abs/1806.07572>

## Tensor Logic, tensor networks, einsum-as-primitive (page 6, part 1.5)

- Pedro Domingos, "Tensor Logic: The Language of AI", arXiv:2510.12269
  (Oct 2025). <https://arxiv.org/abs/2510.12269>
- "Implementing Tensor Logic: Unifying Datalog and Neural Reasoning
  via Tensor Contraction", arXiv:2601.17188 (Jan 2026).
  <https://arxiv.org/abs/2601.17188>
- Earlier in the differentiable-Datalog / neural-symbolic line --
  William W. Cohen, Fan Yang & Kathryn Mazaitis, "TensorLog: A
  Probabilistic Database Implemented as a Deep Neural Network",
  arXiv:1707.05390 (2017). <https://arxiv.org/abs/1707.05390>
- E. Miles Stoudenmire & David J. Schwab, "Supervised Learning with
  Quantum-Inspired Tensor Networks", NIPS 2016; arXiv:1605.05775.
  <https://arxiv.org/abs/1605.05775> ·
  code <https://github.com/emstoudenmire/TNML>
- Alexander Novikov, Dmitrii Podoprikhin, Anton Osokin & Dmitry
  Vetrov, "Tensorizing Neural Networks", NeurIPS 2015;
  arXiv:1509.06569. <https://arxiv.org/abs/1509.06569>
- Ivan Glasser, Nicola Pancotti & J. Ignacio Cirac, "Supervised
  learning with generalized tensor networks", arXiv:1806.05964
  (2018). <https://arxiv.org/abs/1806.05964>
- Ivan V. Oseledets, "Tensor-Train Decomposition", *SIAM Journal on
  Scientific Computing* 33(5):2295-2317 (2011). DOI:
  <https://doi.org/10.1137/090752286>
- Pankaj Mehta & David J. Schwab, "An exact mapping between the
  variational renormalization group and deep learning", 2014;
  arXiv:1410.3831. <https://arxiv.org/abs/1410.3831>
- ITensor library (research workhorse for tensor-network physics):
  <https://itensor.org/>
- Tensor Networks for ML community page: <https://tensornetwork.org/ml/>
- Daniel G. A. Smith & Johnnie Gray, "opt_einsum -- A Python package
  for optimizing contraction order for einsum-like expressions",
  *Journal of Open Source Software* 3(26):753 (2018).
  <https://doi.org/10.21105/joss.00753>
- Alex Rogozhnikov, "Einops: Clear and Reliable Tensor Manipulations
  with Einstein-like Notation", ICLR 2022; <https://einops.rocks>.
  OpenReview: <https://openreview.net/forum?id=oapKSVM2bcj>
- einx universal tensor-notation library:
  <https://github.com/fferflo/einx>

## Modern alignment / RLHF / reasoning (page 6, part 2)

- Paul Christiano et al., "Deep Reinforcement Learning from Human
  Preferences", NeurIPS 2017; arXiv:1706.03741.
  <https://arxiv.org/abs/1706.03741>
- John Schulman et al., "Proximal Policy Optimization Algorithms"
  (PPO), 2017; arXiv:1707.06347. <https://arxiv.org/abs/1707.06347>
- Nisan Stiennon et al., "Learning to summarize from human feedback",
  NeurIPS 2020; arXiv:2009.01325. <https://arxiv.org/abs/2009.01325>
- Long Ouyang et al., "Training language models to follow instructions
  with human feedback" (InstructGPT), NeurIPS 2022; arXiv:2203.02155.
  <https://arxiv.org/abs/2203.02155>
- Yuntao Bai et al., "Constitutional AI: Harmlessness from AI
  Feedback", Anthropic, 2022; arXiv:2212.08073.
  <https://arxiv.org/abs/2212.08073>
- Harrison Lee et al., "RLAIF vs. RLHF: Scaling Reinforcement Learning
  from Human Feedback with AI Feedback", Google, 2023; arXiv:2309.00267.
  <https://arxiv.org/abs/2309.00267>
- Rafael Rafailov, Archit Sharma, Eric Mitchell, Stefano Ermon,
  Christopher D. Manning & Chelsea Finn, "Direct Preference
  Optimization: Your Language Model is Secretly a Reward Model" (DPO),
  NeurIPS 2023; arXiv:2305.18290. <https://arxiv.org/abs/2305.18290>
- Hunter Lightman et al., "Let's Verify Step by Step" (process reward
  models), OpenAI, 2023; arXiv:2305.20050.
  <https://arxiv.org/abs/2305.20050>
- Weizhe Yuan et al., "Self-Rewarding Language Models", 2024;
  arXiv:2401.10020. <https://arxiv.org/abs/2401.10020>
- Kawin Ethayarajh et al., "KTO: Model Alignment as Prospect
  Theoretic Optimization", 2024; arXiv:2402.01306.
  <https://arxiv.org/abs/2402.01306>
- Jiwoo Hong, Noah Lee & James Thorne, "ORPO: Monolithic Preference
  Optimization without Reference Model", 2024; arXiv:2403.07691.
  <https://arxiv.org/abs/2403.07691>
- Yu Meng, Mengzhou Xia & Danqi Chen, "SimPO: Simple Preference
  Optimization with a Reference-Free Reward", 2024; arXiv:2405.14734.
  <https://arxiv.org/abs/2405.14734>
- Arash Ahmadian et al., "Back to Basics: Revisiting REINFORCE-Style
  Optimization for Learning from Human Feedback in LLMs" (RLOO),
  2024; arXiv:2402.14740. <https://arxiv.org/abs/2402.14740>
- Zhihong Shao, Peiyi Wang, Qihao Zhu, Runxin Xu, Junxiao Song et al.,
  "DeepSeekMath: Pushing the Limits of Mathematical Reasoning in Open
  Language Models" (introduces GRPO), 2024; arXiv:2402.03300.
  <https://arxiv.org/abs/2402.03300> ·
  <https://github.com/deepseek-ai/DeepSeek-Math>
- DeepSeek-AI, "DeepSeek-R1: Incentivizing Reasoning Capability in
  LLMs via Reinforcement Learning", 2025; arXiv:2501.12948 ·
  published as "DeepSeek-R1 incentivizes reasoning in LLMs through
  reinforcement learning", *Nature* 645:633-638 (Sep 2025).
  <https://arxiv.org/abs/2501.12948> ·
  <https://www.nature.com/articles/s41586-025-09422-z>
- Sergey Levine, "Reinforcement Learning and Control as Probabilistic
  Inference: Tutorial and Review", 2018; arXiv:1805.00909.
  <https://arxiv.org/abs/1805.00909>

## ARC-AGI ladder, fixed-curriculum environments, auto-curriculum (page 5)

The page-5 north star, the fixed environment set, and the
auto-curriculum / LLM-outer-loop literature.

### ARC-AGI

- Francois Chollet, "On the Measure of Intelligence", 2019;
  arXiv:1911.01547. <https://arxiv.org/abs/1911.01547>
- ARC Prize Foundation, "ARC Prize 2025: Technical Report",
  arXiv:2601.10904. <https://arxiv.org/abs/2601.10904>
- "ARC-AGI-3: A New Challenge for Frontier Agentic Intelligence",
  arXiv:2603.24621 (Mar 2026). <https://arxiv.org/abs/2603.24621> ·
  launch announcement <https://arcprize.org/blog/arc-agi-3-launch> ·
  benchmark page <https://arcprize.org/arc-agi/3>

### Fixed-curriculum environments

- Greg Brockman et al., "OpenAI Gym", 2016; arXiv:1606.01540 (now
  maintained as **Gymnasium** by Farama Foundation,
  <https://gymnasium.farama.org/>). The classic control suite
  baseline.
- Yuval Tassa et al., "DeepMind Control Suite", 2018;
  arXiv:1801.00690. <https://arxiv.org/abs/1801.00690>
- Maxime Chevalier-Boisvert, Lucas Willems & Suman Pal,
  "Minimalistic Gridworld Environment for OpenAI Gym" (MiniGrid),
  2018; <https://github.com/Farama-Foundation/Minigrid>
- Maxime Chevalier-Boisvert et al., "BabyAI: A Platform to Study
  the Sample Efficiency of Grounded Language Learning", ICLR 2019;
  arXiv:1810.08272. <https://arxiv.org/abs/1810.08272>
- Karl Cobbe et al., "Leveraging Procedural Generation to Benchmark
  Reinforcement Learning" (Procgen), ICML 2020; arXiv:1912.01588.
  <https://arxiv.org/abs/1912.01588>
- Danijar Hafner, "Benchmarking the Spectrum of Agent Capabilities"
  (Crafter), 2022; arXiv:2109.06780.
  <https://arxiv.org/abs/2109.06780> ·
  <https://github.com/danijar/crafter>
- Heinrich Kuttler et al., "The NetHack Learning Environment",
  NeurIPS 2020; arXiv:2006.13760.
  <https://arxiv.org/abs/2006.13760>
- Michael Samvelyan et al., "MiniHack the Planet: A Sandbox for
  Open-Ended Reinforcement Learning Research", NeurIPS 2021;
  arXiv:2109.13202. <https://arxiv.org/abs/2109.13202>
- Nolan Bard et al., "The Hanabi Challenge: A New Frontier for AI
  Research", *Artificial Intelligence* 280:103216 (2020);
  arXiv:1902.00506. <https://arxiv.org/abs/1902.00506>
- John P. Agapiou et al., "Melting Pot 2.0", 2022; arXiv:2211.13746.
  <https://arxiv.org/abs/2211.13746>
- Micah Carroll et al., "On the Utility of Learning about Humans
  for Human-AI Coordination" (Overcooked-AI), NeurIPS 2019;
  arXiv:1910.05789. <https://arxiv.org/abs/1910.05789>
- Mohit Shridhar et al., "ALFWorld: Aligning Text and Embodied
  Environments for Interactive Learning", ICLR 2021;
  arXiv:2010.03768. <https://arxiv.org/abs/2010.03768>
- Linxi Fan et al., "MineDojo: Building Open-Ended Embodied Agents
  with Internet-Scale Knowledge", NeurIPS 2022; arXiv:2206.08853.
  <https://arxiv.org/abs/2206.08853>

### Auto-curriculum and open-ended learning

- Rui Wang, Joel Lehman, Jeff Clune & Kenneth O. Stanley, "Paired
  Open-Ended Trailblazer (POET): Endlessly Generating Increasingly
  Complex and Diverse Learning Environments and Their Solutions",
  arXiv:1901.01753 (2019). <https://arxiv.org/abs/1901.01753>
- Rui Wang et al., "Enhanced POET: Open-Ended Reinforcement Learning
  through Unbounded Invention of Learning Challenges and their
  Solutions", ICML 2020; arXiv:2003.08536.
  <https://arxiv.org/abs/2003.08536>
- Minqi Jiang, Edward Grefenstette & Tim Rocktaschel, "Prioritized
  Level Replay" (PLR), ICML 2021; arXiv:2010.03934.
  <https://arxiv.org/abs/2010.03934>
- Jack Parker-Holder, Minqi Jiang, Michael Dennis et al., "Evolving
  Curricula with Regret-Based Environment Design" (ACCEL), ICML
  2022; arXiv:2203.01302. <https://arxiv.org/abs/2203.01302>
- Jenny Zhang, Joel Lehman, Kenneth Stanley & Jeff Clune, "OMNI:
  Open-endedness via Models of human Notions of Interestingness",
  arXiv:2306.01711 (NeurIPS 2024).
  <https://arxiv.org/abs/2306.01711>
- Yecheng Jason Ma et al., "Eureka: Human-Level Reward Design via
  Coding Large Language Models", arXiv:2310.12931 (NVIDIA, 2023).
  <https://arxiv.org/abs/2310.12931>
- Jean-Baptiste Mouret & Jeff Clune, "Illuminating search spaces by
  mapping elites" (MAP-Elites), 2015; arXiv:1504.04909.
  <https://arxiv.org/abs/1504.04909>
- James Kirkpatrick et al., "Overcoming catastrophic forgetting in
  neural networks" (EWC), *PNAS* 114(13):3521-3526 (2017).
  <https://doi.org/10.1073/pnas.1611835114>
- Yoshua Bengio, Jerome Louradour, Ronan Collobert & Jason Weston,
  "Curriculum learning", ICML 2009.
  <https://doi.org/10.1145/1553374.1553380>
- Open-Ended Learning Team (DeepMind), "Open-Ended Learning Leads
  to Generally Capable Agents" (XLand), 2021; arXiv:2107.12808.
  <https://arxiv.org/abs/2107.12808>

### LLM-driven outer loop (Voyager + Sakana templates)

- Guanzhi Wang, Yuqi Xie, Yunfan Jiang, Ajay Mandlekar, Chaowei
  Xiao, Yuke Zhu, Linxi Fan & Anima Anandkumar, "Voyager: An
  Open-Ended Embodied Agent with Large Language Models",
  arXiv:2305.16291 (2023). <https://arxiv.org/abs/2305.16291>
- Sakana AI: AI Scientist v2 (arXiv:2504.08066), DGM
  (arXiv:2505.22954), ShinkaEvolve (arXiv:2509.19349) -- full
  citations in the [Sakana block](#sakana-ai-evolution--collective-intelligence-page-4)
  above.
- Repo-local: the [ralph-loop skill](../../.claude/skills/ralph-loop/SKILL.md)
  is the minimum-viable LLM outer loop already wired into this
  workspace.

## Generative video and game world models (page 7)

Survey companion to [07-video-world-models-survey.md](07-video-world-models-survey.md).
Fast-moving area, May 2026 snapshot; 2601-2605 arXiv IDs are recent
preprints, some not independently verified.

### The SANA efficiency lineage

- Junyu Chen et al., "Deep Compression Autoencoder for Efficient
  High-Resolution Diffusion Models" (DC-AE), arXiv:2410.10733 (2024).
  <https://arxiv.org/abs/2410.10733>
- Enze Xie et al., "SANA: Efficient High-Resolution Image Synthesis
  with Linear Diffusion Transformers", arXiv:2410.10629 (2024); ICLR
  2025. <https://arxiv.org/abs/2410.10629>
- Enze Xie et al., "SANA 1.5: Efficient Scaling of Training-Time and
  Inference-Time Compute in Linear Diffusion Transformer",
  arXiv:2501.18427 (2025). <https://arxiv.org/abs/2501.18427>
- "SANA-Sprint: One-Step Diffusion with Continuous-Time Consistency
  Distillation", arXiv:2503.09641 (2025).
  <https://arxiv.org/abs/2503.09641>
- "SANA-Video: Efficient Video Generation with Block Linear Diffusion
  Transformer", arXiv:2509.24695 (2025).
  <https://arxiv.org/abs/2509.24695>
- Muyang Li et al., "SVDQuant: Absorbing Outliers by Low-Rank
  Components for 4-Bit Diffusion Models", arXiv:2411.05007 (2024);
  ICLR 2025. <https://arxiv.org/abs/2411.05007>
- NVIDIA, "SANA-WM: Efficient Minute-Scale World Modeling with Hybrid
  Linear Diffusion Transformer", arXiv:2605.15178 (2026).
  <https://arxiv.org/abs/2605.15178> ·
  page <https://nvlabs.github.io/Sana/WM/>

### Efficient long-context backbones

- Angelos Katharopoulos et al., "Transformers are RNNs: Fast
  Autoregressive Transformers with Linear Attention", ICML 2020;
  arXiv:2006.16236. <https://arxiv.org/abs/2006.16236>
- Songlin Yang et al., "Gated Linear Attention Transformers with
  Hardware-Efficient Training", arXiv:2312.06635 (2023); ICML 2024.
  <https://arxiv.org/abs/2312.06635>
- Albert Gu & Tri Dao, "Mamba: Linear-Time Sequence Modeling with
  Selective State Spaces", arXiv:2312.00752 (2023).
  <https://arxiv.org/abs/2312.00752>
- Tri Dao & Albert Gu, "Transformers are SSMs: Generalized Models and
  Efficient Algorithms Through Structured State Space Duality"
  (Mamba-2), arXiv:2405.21060 (2024); ICML 2024.
  <https://arxiv.org/abs/2405.21060>
- Songlin Yang, Jan Kautz & Ali Hatamizadeh, "Gated Delta Networks:
  Improving Mamba2 with Delta Rule", arXiv:2412.06464 (2024); ICLR
  2025. <https://arxiv.org/abs/2412.06464>
- Bo Peng et al., "RWKV-7 'Goose' with Expressive Dynamic State
  Evolution", arXiv:2503.14456 (2025).
  <https://arxiv.org/abs/2503.14456>
- Opher Lieber et al., "Jamba: A Hybrid Transformer-Mamba Language
  Model", arXiv:2403.19887 (2024). <https://arxiv.org/abs/2403.19887>
- Yu Sun et al., "Learning to (Learn at Test Time): RNNs with
  Expressive Hidden States" (TTT layers), arXiv:2407.04620 (2024).
  <https://arxiv.org/abs/2407.04620>

### Autoregressive long-video generation and drift

- Boyuan Chen et al., "Diffusion Forcing: Next-Token Prediction Meets
  Full-Sequence Diffusion", arXiv:2407.01392 (2024); NeurIPS 2024.
  <https://arxiv.org/abs/2407.01392>
- Xun Huang et al., "Self Forcing: Bridging the Train-Test Gap in
  Autoregressive Video Diffusion", arXiv:2506.08009 (2025).
  <https://arxiv.org/abs/2506.08009>
- Tianwei Yin et al., "From Slow Bidirectional to Fast Autoregressive
  Video Diffusion Models" (CausVid), arXiv:2412.07772 (2024); CVPR
  2025. <https://arxiv.org/abs/2412.07772>
- Sand AI, "MAGI-1: Autoregressive Video Generation at Scale",
  arXiv:2505.13211 (2025). <https://arxiv.org/abs/2505.13211>
- Skywork, "SkyReels-V2: Infinite-Length Film Generative Model",
  arXiv:2504.13074 (2025). <https://arxiv.org/abs/2504.13074>
- "LongLive: Real-time Interactive Long Video Generation",
  arXiv:2509.22622 (2025). <https://arxiv.org/abs/2509.22622>
- Lvmin Zhang & Maneesh Agrawala, "Packing Input Frame Context in
  Next-Frame Prediction Models for Video Generation" (FramePack),
  arXiv:2504.12626 (2025). <https://arxiv.org/abs/2504.12626>

### Interactive and playable world models

- Dani Valevski et al., "Diffusion Models Are Real-Time Game Engines"
  (GameNGen), arXiv:2408.14837 (2024).
  <https://arxiv.org/abs/2408.14837>
- Jake Bruce et al., "Genie: Generative Interactive Environments",
  arXiv:2402.15391 (2024); ICML 2024.
  <https://arxiv.org/abs/2402.15391>
- DeepMind, "Genie 2: A large-scale foundation world model" (Dec
  2024) and "Genie 3: A new frontier for world models" (Aug 2025),
  blog/tech-report releases.
- Decart & Etched, "Oasis: A Universe in a Transformer" (open-source
  Minecraft diffusion world model, 2024).
- Eloi Alonso et al., "Diffusion for World Modeling: Visual Details
  Matter in Atari" (DIAMOND), arXiv:2405.12399 (2024); NeurIPS 2024
  spotlight. <https://arxiv.org/abs/2405.12399>
- "Matrix-Game: Interactive World Foundation Model",
  arXiv:2506.18701 (2025). <https://arxiv.org/abs/2506.18701> ·
  Matrix-Game 2.0 arXiv:2508.13009 · 3.0 arXiv:2604.08995
- Haoxuan Che et al., "GameGen-X: Interactive Open-world Game Video
  Generation", arXiv:2411.00769 (2024); ICLR 2025.
  <https://arxiv.org/abs/2411.00769>
- Tencent, "Hunyuan-GameCraft: High-dynamic Interactive Game Video
  Generation", arXiv:2506.17201 (2025).
  <https://arxiv.org/abs/2506.17201>
- Microsoft Research & Ninja Theory, "World and Human Action Models
  towards gameplay ideation" (Muse / WHAM), *Nature* 638 (2025).
  <https://doi.org/10.1038/s41586-025-08600-3>

### Control, memory, and geometric grounding

- Hao He et al., "CameraCtrl: Enabling Camera Control for
  Text-to-Video Generation", arXiv:2404.02101 (2024).
  <https://arxiv.org/abs/2404.02101>
- Zhouxia Wang et al., "MotionCtrl: A Unified and Flexible Motion
  Controller for Video Generation", arXiv:2312.03641 (2023).
  <https://arxiv.org/abs/2312.03641>
- Dejia Xu et al., "CamCo: Camera-Controllable 3D-Consistent
  Image-to-Video Generation", arXiv:2406.02509 (2024).
  <https://arxiv.org/abs/2406.02509>
- Wangbo Yu et al., "ViewCrafter: Taming Video Diffusion Models for
  High-fidelity Novel View Synthesis", arXiv:2409.02048 (2024).
  <https://arxiv.org/abs/2409.02048>
- Stability AI, "Stable Virtual Camera: Generative View Synthesis
  with Diffusion Models" (SEVA), arXiv:2503.14489 (2025).
  <https://arxiv.org/abs/2503.14489>
- Sherwin Bahmani et al., "AC3D: Analyzing and Improving 3D Camera
  Control in Video Diffusion Transformers", arXiv:2411.18673 (2024);
  CVPR 2025. <https://arxiv.org/abs/2411.18673>
- "GS-DiT: Advancing Video Generation with Pseudo 4D Gaussian Fields",
  arXiv:2501.02690 (2025). <https://arxiv.org/abs/2501.02690>
- Yifan Wang & Tong He, "Warp-as-History: Generalizable
  Camera-Controlled Video Generation from One Training Video",
  arXiv:2605.15182 (2026). <https://arxiv.org/abs/2605.15182> ·
  page <https://yyfz.github.io/warp-as-history/>
- Zeqi Xiao et al., "WorldMem: Long-term Consistent World Simulation
  with Memory", arXiv:2504.12369 (2025).
  <https://arxiv.org/abs/2504.12369>
- "Video World Models with Long-term Spatial Memory",
  arXiv:2506.05284 (2025). <https://arxiv.org/abs/2506.05284>
- "WorldWarp: Asynchronous 3D Video Diffusion", arXiv:2512.19678
  (2025). <https://arxiv.org/abs/2512.19678>
- Haoyi Zhu et al., "Aether: Geometric-Aware Unified World Modeling",
  arXiv:2503.18945 (2025); ICCV 2025.
  <https://arxiv.org/abs/2503.18945>

### Embodied and predictive world models

- Niket Agarwal et al. (NVIDIA), "Cosmos World Foundation Model
  Platform for Physical AI", arXiv:2501.03575 (2025).
  <https://arxiv.org/abs/2501.03575>
- Meta FAIR, "V-JEPA 2: Self-Supervised Video Models Enable
  Understanding, Prediction and Planning", arXiv:2506.09985 (2025).
  <https://arxiv.org/abs/2506.09985>
- Amir Bar et al., "Navigation World Models", arXiv:2412.03572
  (2024); CVPR 2025. <https://arxiv.org/abs/2412.03572>
- Shenyuan Gao et al. (NVIDIA), "DreamDojo: A Generalist Robot World
  Model", arXiv:2602.06949 (2026).
  <https://arxiv.org/abs/2602.06949>
- Anthony Hu et al. (Wayve), "GAIA-1: A Generative World Model for
  Autonomous Driving", arXiv:2309.17080 (2023).
  <https://arxiv.org/abs/2309.17080> · GAIA-2 arXiv:2503.20523
- Shenyuan Gao et al., "Vista: A Generalizable Driving World Model
  with High Fidelity and Versatile Controllability", arXiv:2405.17398
  (2024); NeurIPS 2024. <https://arxiv.org/abs/2405.17398>

### SANA-WM competitor world models

- "LingBot-World: Advancing Open-source World Models",
  arXiv:2601.20540 (2026).
- "HY-WorldPlay: Towards Long-Term Geometric Consistency for
  Real-Time Interactive World Modeling", arXiv:2512.14614 (2025).
- "Infinite-World: Scaling Interactive World Models to 1000-Frame
  Horizons via Pose-Free Hierarchical Memory", arXiv:2602.02393
  (2026).
- "Matrix-Game 3.0: Real-time Streaming World Model",
  arXiv:2604.08995 (2026).

## ARC-AGI-3 and the exploration / goal-inference literature (page 9)

Survey companion to
[09-arc-agi3-exploration-survey.md](09-arc-agi3-exploration-survey.md).
ARC-AGI-3 launched March 2026; the benchmark-specific literature is
thin (one challenge paper, one prior prize report, one preview
competition, one method preprint). The exploration / meta-RL items
are mostly mature pre-2022 work the thvm arc would port.

### ARC-AGI-3 and the ARC Prize

- Francois Chollet, Mike Knoop et al., "ARC-AGI-3: A New Challenge
  for Frontier Agentic Intelligence", arXiv:2603.24621 (Mar 2026).
  <https://arxiv.org/abs/2603.24621> · technical report
  <https://arcprize.org/media/ARC_AGI_3_Technical_Report.pdf> ·
  benchmark page <https://arcprize.org/arc-agi/3>
- ARC Prize Foundation, "ARC Prize 2025: Technical Report",
  arXiv:2601.10904 (Jan 2026). <https://arxiv.org/abs/2601.10904>
- ARC Prize Foundation, "ARC-AGI-3 Preview: 30-Day Learnings"
  (blog/tech report, 2025).
  <https://arcprize.org/blog/arc-agi-3-preview-30-day-learnings>
- Dries Smit (Tufa Labs), "StochasticGoose" -- 1st place in the
  ARC-AGI-3 Preview Agent Competition; CNN action-affordance
  predictor (12.58% preview, 0.25% full benchmark). Write-up
  <https://medium.com/@dries.epos/1st-place-in-the-arc-agi-3-agent-preview-competition-49263f6287db> ·
  code <https://github.com/DriesSmit/ARC3-solution>
- Evgenii Rudakov, Jonathan Shock & Benjamin Ultan Cowley,
  "Graph-Based Exploration for ARC-AGI-3 Interactive Reasoning
  Tasks", arXiv:2512.24156 (2025).
  <https://arxiv.org/abs/2512.24156>

### Exploration under sparse or absent reward

- Deepak Pathak, Pulkit Agrawal, Alexei A. Efros & Trevor Darrell,
  "Curiosity-driven Exploration by Self-supervised Prediction" (ICM),
  ICML 2017; arXiv:1705.05363. <https://arxiv.org/abs/1705.05363>
  (also cited in the breakthrough-2 block above.)
- Yuri Burda, Harrison Edwards, Amos Storkey & Oleg Klimov,
  "Exploration by Random Network Distillation" (RND),
  arXiv:1810.12894 (2018). <https://arxiv.org/abs/1810.12894>
- Marc G. Bellemare, Sriram Srinivasan, Georg Ostrovski, Tom Schaul,
  David Saxton & Remi Munos, "Unifying Count-Based Exploration and
  Intrinsic Motivation", NeurIPS 2016; arXiv:1606.01868.
  <https://arxiv.org/abs/1606.01868>
- Adrien Ecoffet, Joost Huizinga, Joel Lehman, Kenneth O. Stanley &
  Jeff Clune, "First return, then explore" (Go-Explore), *Nature*
  590:580-586 (2021); arXiv:1901.10995.
  <https://doi.org/10.1038/s41586-020-03157-9> ·
  <https://arxiv.org/abs/1901.10995>
- Adria Puigdomenech Badia, Pablo Sprechmann, Alex Vitvitskyi et al.,
  "Never Give Up: Learning Directed Exploration Strategies" (NGU),
  ICLR 2020; arXiv:2002.06038. <https://arxiv.org/abs/2002.06038>
- Adria Puigdomenech Badia et al., "Agent57: Outperforming the Atari
  Human Benchmark", arXiv:2003.13350 (2020).
  <https://arxiv.org/abs/2003.13350> (also cited for page 8.)

### Learning progress, automatic curriculum, and proposer collapse

- Celeste Kidd, Steven T. Piantadosi & Richard N. Aslin, "The
  Goldilocks Effect: Human Infants Allocate Attention to Visual
  Sequences That Are Neither Too Simple Nor Too Complex", *PLoS ONE*
  7(5):e36399 (2012). <https://doi.org/10.1371/journal.pone.0036399>
- Pierre-Yves Oudeyer & Frederic Kaplan, "What is Intrinsic
  Motivation? A Typology of Computational Approaches", *Frontiers in
  Neurorobotics* 1:6 (2007).
  <https://doi.org/10.3389/neuro.12.006.2007>
- Jurgen Schmidhuber, "Formal Theory of Creativity, Fun, and
  Intrinsic Motivation (1990-2010)", *IEEE Trans. Autonomous Mental
  Development* 2(3):230-247 (2010).
  <https://doi.org/10.1109/TAMD.2010.2056368>
- Marcelo G. Mattar & Nathaniel D. Daw, "Prioritized memory access
  explains planning and hippocampal replay", *Nature Neuroscience*
  21:1609-1617 (2018). <https://doi.org/10.1038/s41593-018-0232-z>
- Amitai Shenhav, Matthew M. Botvinick & Jonathan D. Cohen, "The
  Expected Value of Control: An Integrative Theory of Anterior
  Cingulate Cortex Function", *Neuron* 79(2):217-240 (2013).
  <https://doi.org/10.1016/j.neuron.2013.07.007>
- Luke Bailey, Kaiyue Wen, Kefan Dong, Tatsunori Hashimoto & Tengyu
  Ma, "Scaling Self-Play with Self-Guidance" (Self-Guided Self-Play,
  SGS), arXiv:2604.20209 (2026). <https://arxiv.org/abs/2604.20209>
- Naftali Tishby, Fernando C. Pereira & William Bialek, "The
  Information Bottleneck Method", 1999; arXiv:physics/0004057.
  <https://arxiv.org/abs/physics/0004057> (rate-distortion backbone of
  the compression / MDL goal-prior -- maximise structure subject to
  preserving conserved content, avoiding the blank-state collapse).

### Goal inference, skill discovery, empowerment

- Andrew Y. Ng & Stuart Russell, "Algorithms for Inverse
  Reinforcement Learning", ICML 2000.
  <https://ai.stanford.edu/~ang/papers/icml00-irl.pdf> (also cited
  in the mentalizing block above.)
- Brian D. Ziebart, Andrew Maas, J. Andrew Bagnell & Anind K. Dey,
  "Maximum Entropy Inverse Reinforcement Learning", AAAI 2008.
  <https://www.aaai.org/Papers/AAAI/2008/AAAI08-227.pdf> (also cited
  in the mentalizing block above.)
- Karl Gregor, Danilo Jimenez Rezende & Daan Wierstra, "Variational
  Intrinsic Control" (VIC), arXiv:1611.07507 (2016).
  <https://arxiv.org/abs/1611.07507>
- Benjamin Eysenbach, Abhishek Gupta, Julian Ibarz & Sergey Levine,
  "Diversity is All You Need: Learning Skills without a Reward
  Function" (DIAYN), arXiv:1802.06070 (2018).
  <https://arxiv.org/abs/1802.06070>
- Jiaheng Hu, Zizhao Wang, Peter Stone & Roberto Martin-Martin,
  "Disentangled Unsupervised Skill Discovery for Efficient
  Hierarchical Reinforcement Learning" (DUSDi), NeurIPS 2024;
  arXiv:2410.11251. <https://arxiv.org/abs/2410.11251>
- Alexander S. Klyubin, Daniel Polani & Chrystopher L. Nehaniv,
  "Empowerment: A Universal Agent-Centric Measure of Control", IEEE
  Congress on Evolutionary Computation, 2005.
  <https://doi.org/10.1109/CEC.2005.1554676>
- "A Unified Bellman Optimality Principle Combining Reward
  Maximization and Empowerment", arXiv:1907.12392 (2019).
  <https://arxiv.org/abs/1907.12392>
- "Towards Empowerment Gain through Causal Structure Learning in
  Model-Based RL", arXiv:2502.10077 (2025).
  <https://arxiv.org/abs/2502.10077>

### Few-shot transfer, meta-RL, in-context RL

- Jane X. Wang et al., "Learning to Reinforcement Learn",
  arXiv:1611.05763 (2016). <https://arxiv.org/abs/1611.05763>
  (also cited in the meta-RL block above.)
- Yan Duan, John Schulman, Xi Chen, Peter L. Bartlett, Ilya
  Sutskever & Pieter Abbeel, "RL^2: Fast Reinforcement Learning via
  Slow Reinforcement Learning", arXiv:1611.02779 (2016).
  <https://arxiv.org/abs/1611.02779>
- Chelsea Finn, Pieter Abbeel & Sergey Levine, "Model-Agnostic
  Meta-Learning for Fast Adaptation of Deep Networks" (MAML), ICML
  2017; arXiv:1703.03400. <https://arxiv.org/abs/1703.03400>
- Michael Laskin, Luyu Wang, Junhyuk Oh et al., "In-context
  Reinforcement Learning with Algorithm Distillation",
  arXiv:2210.14215 (2022). <https://arxiv.org/abs/2210.14215>
- Jonathan N. Lee, Annie Xie, Aldo Pacchiano, Yash Chandak, Chelsea
  Finn, Ofir Nachum & Emma Brunskill, "Supervised Pretraining Can
  Learn In-Context Reinforcement Learning" (Decision-Pretrained
  Transformer), NeurIPS 2023; arXiv:2306.14892.
  <https://arxiv.org/abs/2306.14892>
- "Towards Large-Scale In-Context Reinforcement Learning by
  Meta-Training in Randomized Worlds", arXiv:2502.02869 (2025).
  <https://arxiv.org/abs/2502.02869>
- "Distilling Reinforcement Learning Algorithms for In-Context
  Model-Based Planning" (DICP), arXiv:2502.19009 (2025).
  <https://arxiv.org/abs/2502.19009>
- Open-Ended Learning Team (DeepMind), "Open-Ended Learning Leads
  to Generally Capable Agents" (XLand), arXiv:2107.12808 (2021).
  <https://arxiv.org/abs/2107.12808> (also cited in the
  auto-curriculum block above.)

## Neuro-symbolic, differentiable-logic, and program-synthesis ARC solvers (page 9 follow-up, 2026-06-17)

Gathered from a verified literature survey prompted by the question "did anyone try
differentiable logic / memory-augmented (neuromorphic) architectures to learn symbolic
rules for ARC-AGI?". Short answer: differentiable-logic and memory-augmented models have
been tried only on ARC-adjacent tasks and are not competitive; the working approaches are
discrete program search (DSL/constraint/library) optionally guided by neural models.

- Richard Evans & Edward Grefenstette, "Learning Explanatory Rules from Noisy Data"
  (Differentiable Inductive Logic, ∂ILP), JAIR 61, 2018; arXiv:1711.04574. Foundational
  dILP; memory-bound (arity-2 predicates), never run on ARC.
  <https://arxiv.org/abs/1711.04574>
- Felix Petersen, Christian Borgelt, Hilde Kuehne & Oliver Deussen, "Deep Differentiable
  Logic Gate Networks", NeurIPS 2022; arXiv:2210.08277. Image/tabular classification only;
  no ARC. <https://arxiv.org/abs/2210.08277>
- Hikaru Shindo et al., "Learning Differentiable Logic Programs for Abstract Visual
  Reasoning" (NEUMANN, differentiable forward-chaining FOL reasoner), Machine Learning
  journal, 2023; arXiv:2307.00928. Kandinsky / CLEVR-Hans, not ARC.
  <https://arxiv.org/abs/2307.00928>
- Mattia Atzeni, Mrinmaya Sachan & Andreas Loukas, "Infusing Lattice Symmetry Priors in
  Attention Mechanisms for Sample-Efficient Abstract Geometric Reasoning" (LatFormer),
  ICML 2023. The most ARC-relevant differentiable result: lattice-symmetry priors in
  attention, ~2 orders of magnitude better sample efficiency on geometric grid transforms
  from ARC (beats a DNC baseline that scored ~0); scoped to the geometric subset, no
  full-ARC score. <https://proceedings.mlr.press/v202/atzeni23a/atzeni23a.pdf>
- Yudong Xu, Elias B. Khalil & Scott Sanner, "Graphs, Constraints, and Search for the
  Abstraction and Reasoning Corpus" (ARGA), AAAI 2023. Object-graph abstraction +
  constraint-guided search over a small relational DSL (4 filters + 11 transforms);
  57/160 with ~1000x fewer search nodes than the Kaggle winner. The strongest non-LLM
  neuro-symbolic route. <https://ssanner.github.io/papers/aaai23_arga.pdf>
- Mikel Bober-Irizar & Soumya Banerjee, "Neural networks for abstraction and reasoning"
  (DreamCoder + the PeARL DSL), Scientific Reports 14, 2024. 70/400 (easy), 18/400 (hard);
  neuro-symbolic and LLM solvers are complementary (37% overlap).
  <https://www.nature.com/articles/s41598-024-73582-7>
- Natasha Butt, Blazej Manczak, Auke Wiggers et al. (Qualcomm AI Research), "CodeIt:
  Self-Improving Language Models with Prioritized Hindsight Replay", ICML 2024;
  arXiv:2402.04858. LM + expert iteration + hindsight relabeling + prioritized replay for
  reward sparsity; 59/400 (15%). Mirrors the goal-oversampling used in brain experiment
  348. <https://arxiv.org/abs/2402.04858>
- Wen-Ding Li, Kevin Ellis et al., "Combining Induction and Transduction for Abstract
  Reasoning" (BARC), ICLR 2025; arXiv:2411.02272. Induction (program synthesis) +
  transduction (neural prediction) ensemble, 56.75% on the 400-task public validation
  (~average human 60.2%). <https://arxiv.org/abs/2411.02272>
- Ekin Akyürek et al., "The Surprising Effectiveness of Test-Time Training for Abstract
  Reasoning", arXiv:2411.07279 (2024). TTT lifts BARC's transduction model to 61.9%
  ensembled; TTT complements but does not substitute for program synthesis.
  <https://arxiv.org/abs/2411.07279>
- Yuval Shamshoum et al., "Differentiable Neural Computers Require More Planning Steps",
  ICML 2024; arXiv:2406.02187. Adaptive planning budget for DNCs -- on algorithmic tasks,
  not ARC. <https://arxiv.org/abs/2406.02187>
- Alex Graves et al., "Hybrid computing using a neural network with dynamic external
  memory" (Differentiable Neural Computer), Nature 538, 2016. The memory-augmented
  archetype; the LatFormer DNC baseline scored ~0 on the ARC geometric subset.
  <https://www.nature.com/articles/nature20101>
- Johan Sokrates Wind ("Icecuber"), 1st-place Kaggle ARC 2020 solution: hand-crafted DSL
  (~42 image-transformation functions) + DAG brute-force stacker. Still the strongest
  single solver (~209/400 easy, 160/400 hard in the Nature re-evaluation).
  <https://www.kaggle.com/c/abstraction-and-reasoning-challenge/discussion/154597>

## Modern robotics research (page 12)

The 2022-present robotics surge, surveyed for brain-arc importable
mechanisms (page 12, 12-modern-robotics-survey.md).

World models that learn and deploy:

- Danijar Hafner et al., "Mastering Diverse Domains through World
  Models" (Dreamer V3), arXiv:2301.04104 (2023).
  <https://arxiv.org/abs/2301.04104>
- Philipp Wu et al., "DayDreamer: World Models for Physical Robot
  Learning", arXiv:2206.14176 (2022). <https://arxiv.org/abs/2206.14176>
- Jake Bruce et al., "Genie: Generative Interactive Environments", ICML
  2024; arXiv:2402.15391. <https://arxiv.org/abs/2402.15391>
  (Genie 2 followup, DeepMind blog 2024.)
- Vincent Micheli et al., "Transformers are Sample-Efficient World
  Models" (IRIS), ICLR 2023; arXiv:2209.00588 (also cited in page 11).
  <https://arxiv.org/abs/2209.00588>

Diffusion / flow action policies:

- Cheng Chi et al., "Diffusion Policy: Visuomotor Policy Learning via
  Action Diffusion", RSS 2023; arXiv:2303.04137.
  <https://arxiv.org/abs/2303.04137>
- Anurag Ajay et al., "Is Conditional Generative Modeling all you need
  for Decision-Making?" (Decision Diffuser), arXiv:2211.15657 (2023).
  <https://arxiv.org/abs/2211.15657>
- Jacob Austin et al., "Structured Denoising Diffusion Models in
  Discrete State-Spaces" (D3PM, the discrete-diffusion substrate for
  tokenized action spaces), NeurIPS 2021; arXiv:2107.03006.
  <https://arxiv.org/abs/2107.03006>

Action chunking and temporal abstraction:

- Tony Z. Zhao et al., "Learning Fine-Grained Bimanual Manipulation
  with Low-Cost Hardware" (ACT, ALOHA), RSS 2023; arXiv:2304.13705.
  <https://arxiv.org/abs/2304.13705>
- Konstantinos Bousmalis et al., "RoboCat: A Self-Improving Foundation
  Agent for Robotic Manipulation", arXiv:2306.11706 (2023).
  <https://arxiv.org/abs/2306.11706>
- Zipeng Fu et al., "Mobile ALOHA: Learning Bimanual Mobile
  Manipulation with Low-Cost Whole-Body Teleoperation", arXiv:2401.02117
  (2024). <https://arxiv.org/abs/2401.02117>

Latent action pretraining (the closest cousin to the page-11 attention
follow-up):

- Seonghyeon Ye et al., "Latent Action Pretraining from Videos" (LAPA),
  arXiv:2410.11758 (2024). <https://arxiv.org/abs/2410.11758>
- Jake Bruce et al., Genie (cited above).

Vision-language-action / cross-embodiment foundation models (LANDSCAPE
ONLY, OUT OF SCOPE for the brain-arc per project constraint):

- Anthony Brohan et al., "RT-1: Robotics Transformer for Real-World
  Control at Scale", arXiv:2212.06817 (2022).
  <https://arxiv.org/abs/2212.06817>
- Anthony Brohan et al., "RT-2: Vision-Language-Action Models Transfer
  Web Knowledge to Robotic Control", arXiv:2307.15818 (2023).
  <https://arxiv.org/abs/2307.15818>
- Abhishek Padalkar et al., "Open X-Embodiment: Robotic Learning
  Datasets and RT-X Models", arXiv:2310.08864 (2024).
  <https://arxiv.org/abs/2310.08864>
- Moo Jin Kim et al., "OpenVLA: An Open-Source Vision-Language-Action
  Model", arXiv:2406.09246 (2024). <https://arxiv.org/abs/2406.09246>
- Kevin Black et al., "pi0: A Vision-Language-Action Flow Model for
  General Robot Control", arXiv:2410.24164 (2024).
  <https://arxiv.org/abs/2410.24164>

Visual representations for action:

- Suraj Nair et al., "R3M: A Universal Visual Representation for Robot
  Manipulation", arXiv:2203.12601 (2023).
  <https://arxiv.org/abs/2203.12601>
- Arjun Majumdar et al., "Where are we in the search for an Artificial
  Visual Cortex for Embodied Intelligence?" (VC-1), NeurIPS 2023;
  arXiv:2303.18240. <https://arxiv.org/abs/2303.18240>
- Tete Xiao et al., "Masked Visual Pre-training for Motor Control"
  (MVP), arXiv:2210.03109 (2022). <https://arxiv.org/abs/2210.03109>

Skill / option pretraining (closest cousin to page 11):

- Avi Singh et al., "PARROT: Data-Driven Behavioral Priors for
  Reinforcement Learning", ICLR 2021; arXiv:2011.10024.
  <https://arxiv.org/abs/2011.10024>
- Eric Jang et al., "BC-Z: Zero-Shot Task Generalization with Robotic
  Imitation Learning", arXiv:2202.02005 (2022).
  <https://arxiv.org/abs/2202.02005>

Neuroscience grounding (page 12 connections):

- Karl Friston, "The free-energy principle: a unified brain theory?",
  Nature Reviews Neuroscience 2010; predictive coding.
  <https://www.nature.com/articles/nrn2787>
- Andre M. Bastos et al., "Canonical Microcircuits for Predictive
  Coding", Neuron 76(4), 2012.
  <https://www.cell.com/neuron/fulltext/S0896-6273(12)00892-1>
- Mark M. Churchland et al., "Neural population dynamics during
  reaching" (motor cortex as a dynamical system; action-distribution
  view relevant to diffusion policies), Nature 487, 2012.
  <https://www.nature.com/articles/nature11129>
- Emilio Bizzi & Andrea d'Avella, "Modular organization of the spinal
  motor system" (motor primitives / muscle synergies relevant to
  latent action pretraining).
- Daniel M. Wolpert et al., "Principles of sensorimotor learning"
  (cerebellar adaptation), Nature Reviews Neuroscience 12, 2011.
  <https://www.nature.com/articles/nrn3112>

## thvm internal docs referenced

- [../grad.md](../grad.md) -- autodiff.
- [../wl.md](../wl.md) -- the Wolfram LibraryLink surface.
- [../cpu.md](../cpu.md), [../metal.md](../metal.md) -- backends.
- `wl/THVMLink/Kernel/NN.wl`, `wl/THVMLink/Kernel/Optim.wl` -- NN
  primitives and `TAdam`.
- `wl/Examples/` -- `linear-train`, `mlp-mnist`, `lenet-mnist`,
  `beautiful-mnist`, `gpt2`, `newton-1d`.
- `docs/plans/beautiful_mnist_parity.md` -- MNIST training status.
