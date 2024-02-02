# Tips For Contributing

### View And Create Issues

Everyone in our team at any time can add new issues to our project. These are things like missing features and bug reports. To view and create issues, first go to the project git repository on GitHub and click on the "Issues" tab. There it will present you with all the currently open issues. To add a new issue, click the "New issue" button. You should give your new issue a short title and a description. Optionally, you can add some labels you think are appropriate.

Once created, new issues are automatically added to the roadmap. On the right side of the new issue's page under the "Projects" field, you can assign the issue to a team and set a start and end date. You can view the roadmap by clicking on the "Projects" tab and then "Bangnet Roadmap".

### Work On Issue

To start work on an issue that has been assigned to you, you need to create a new branch just for that issue. If the issue number is #27 and the name is "Fix README File", then you should name your branch “27-fix-readme-file”. Creating a new branch depends on how you use git so have a look online. For me, I enter the following command within the cloned repository:

```sh
git switch -c 27-fix-readme-file
```

Then you can go ahead and do your work. Remember to stage and commit frequently, especially if you are making a big change. For me, that means these commands:

```sh
git add README.md
git commit -m "Fix the readme file"
```

Then when you are ready to save your commits to the online GitHub repository, you need to push your commits. Doing this allows others to see your progress. For me, that looks like:

```sh
git push -u origin 27-fix-readme-file
# After first push to a new branch, all subsequent pushes are just
git push
```

### Create Pull Requests

Once you are happy with your code and think it should be merged into the real codebase, you need to create a pull request on GitHub. Click on the "Pull requests" tab in the online GitHub repository and click "New pull request", then select the branch you want to merge.

In this screen you can compare what has been changed and add additional information. You should link the issue you were working on to this new pull request by either [using closing keywords or manually after you have created the pull request](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue#linking-a-pull-request-to-an-issue-using-a-keyword). This automatically moves the issue from the In Progress to the Done column when the pull request is merged into the master branch. If there are no issues, such as [two people trying to edit the same file before both merging](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/addressing-merge-conflicts/resolving-a-merge-conflict-on-github), you should be able to click "Create pull request".

After creating the pull request, you need to get someone to review your code before we merge it into the real codebase. In the meantime, you can get started on your next issue. Once the reviewer(s) have reviewed the code, they can accept the pull request which will merge your branch into the master branch.
